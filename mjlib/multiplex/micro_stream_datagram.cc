// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mjlib/multiplex/micro_stream_datagram.h"

#include <cstddef>
#include <functional>

#include <boost/crc.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/buffer_stream.h"

#include "mjlib/micro/async_stream.h"

#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/micro_error.h"
#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

namespace pl = std::placeholders;
using BufferReadStream = multiplex::ReadStream<base::BufferReadStream>;
using BufferWriteStream = multiplex::WriteStream<base::BufferWriteStream>;

class MicroStreamDatagram::Impl : public Format {
 public:
  Impl(micro::Pool* pool, micro::AsyncStream* stream, const Options& options)
      : stream_(stream),
        options_(options),
        read_buffer_(static_cast<char*>(
                         pool->Allocate(options.buffer_size, 1))),
        write_buffer_(static_cast<char*>(
                          pool->Allocate(options.buffer_size, 1))) {
  }

  void AsyncRead(Header* header, const base::string_span& data,
                 const micro::SizeCallback& callback) {
    MJ_ASSERT(!current_read_callback_);
    current_read_callback_ = callback;
    current_read_header_ = header;
    current_read_data_ = data;

    // See if we already have enough data.
    if (TryEmitOneFrame()) {
      // All good.
      return;
    }

    // Nope, we need to start reading.
    StartReadFrame();
  }

  void AsyncWrite(const Header& header, const std::string_view& data,
                  const micro::SizeCallback& callback) {
    MJ_ASSERT(!current_write_callback_);
    current_write_callback_ = callback;
    current_write_size_ = data.size();

    // First, figure out how big our size varuint will end up being.
    const auto header_size =
        2 + // kHeader
        1 + // source id
        1 + // dest_id
        GetVaruintSize(data.size());

    constexpr int kCrcSize = 2;

    MJ_ASSERT((header_size + kCrcSize + data.size()) < options_.buffer_size);
    std::memcpy(&write_buffer_[header_size], data.data(), data.size());

    {
      base::BufferWriteStream header_buffer_stream(
          base::string_span(write_buffer_, header_size));
      BufferWriteStream header_stream(header_buffer_stream);
      header_stream.Write(kHeader);
      header_stream.Write(static_cast<uint8_t>(header.source));
      header_stream.Write(static_cast<uint8_t>(header.destination));
      header_stream.WriteVaruint(data.size());
    }

    // Now figure out the checksum.
    const auto crc_location = header_size + data.size();
    boost::crc_ccitt_type crc;
    crc.process_bytes(write_buffer_, crc_location);
    const uint16_t actual_crc = crc.checksum();

    {
      base::BufferWriteStream crc_buffer_stream(
          base::string_span(&write_buffer_[crc_location], 2));
      BufferWriteStream crc_stream(crc_buffer_stream);
      crc_stream.Write(actual_crc);
    }

    async_writer_.Write(
        *stream_,
        std::string_view(write_buffer_, crc_location + 2),
        std::bind(&Impl::HandleWrite, this, std::placeholders::_1));
  }

  void HandleWrite(const micro::error_code& ec) {
    auto copy = current_write_callback_;
    auto bytes = current_write_size_;

    current_write_callback_ = {};
    current_write_size_ = 0;

    copy(ec, bytes);
  }

  void StartReadFrame() {
    stream_->AsyncReadSome(
        base::string_span(&read_buffer_[read_start_],
                          options_.buffer_size - read_start_),
        std::bind(&Impl::HandleReadFrame, this,
                  pl::_1, pl::_2));
  }

  void HandleReadFrame(const micro::error_code& ec, size_t size) {
    if (ec) {
      read_start_ = 0;
      InvokeReadCallback(ec, 0);
      return;
    }

    read_start_ += size;

    if (TryEmitOneFrame()) {
      // We're all done now.
      return;
    }

    // Read more still.
    StartReadFrame();
  }

  /// Return true if we successfully emitted one frame.
  bool TryEmitOneFrame() __attribute__ ((optimize("O3"))) {
    // For now, we'll require that we fit a whole valid packet into
    // our buffer at once before acting on it.  So we'll just keep
    // calling StartReadFrame until we get there.

    // Work to start our buffer out with the frame header.

    constexpr char kHeaderLowByte = (kHeader & 0xff);

    // This is basically memchr, but executes a lot faster.
    char* const found = [&]() -> char* {
      for (int i = 0; i < read_start_; i++) {
        if (read_buffer_[i] == kHeaderLowByte) {
          return read_buffer_ + i;
        }
      }
      return nullptr;
    }();

    if (found == nullptr) {
      // We don't have anything which could be a header in our
      // buffer.  Just wipe it all out and start over.
      read_start_ = 0;
      return false;
    }

    Consume(found - read_buffer_);

    if (read_start_ < 2) {
      // We need more to even have a header.
      return false;
    }

    if (static_cast<uint8_t>(read_buffer_[1]) != ((kHeader >> 8) & 0xff)) {
      // We had the first byte of a header, but not the second byte.

      // Move out this false start and try again.
      Consume(2);
      return true;
    }

    // We need at least 7 bytes to have a minimal frame.
    if (read_start_ < (kHeaderSize + kCrcSize + kMinVaruintSize)) {
      return false;
    }

    // See if we have enough data to have a valid varuint for size.
    base::BufferReadStream data({&read_buffer_[2],
            static_cast<size_t>(read_start_ - 2)});
    BufferReadStream read_stream{data};

    const auto maybe_source_id = read_stream.Read<uint8_t>();
    const auto maybe_dest_id = read_stream.Read<uint8_t>();
    const auto maybe_size = read_stream.ReadVaruint();

    if (!maybe_size) {
      // We don't have enough for the size yet.
      return false;
    }

    const auto payload_size = *maybe_size;
    if (payload_size > 4096) {
      // We'll claim this is guaranteed to be too big.  Just wipe
      // everything out and start over.
      read_start_ = 0;
      return false;
    }

    if (payload_size > (options_.buffer_size - 7))  {
      // We can't fit this either.
      read_start_ = 0;
      return false;
    }

    // Do we have enough data yet?
    const char* const payload_start = data.position();
    const auto data_we_have =
        &read_buffer_[read_start_] - data.position();
    if (data_we_have < static_cast<std::ptrdiff_t>(payload_size + 2)) {
      // We need more still.
      return false;
    }

    const char* const crc_location =
        data.position() + payload_size;

    // Woohoo.  We nominally have enough for a whole frame.  Verify
    // the checksum!
    boost::crc_ccitt_type crc;
    crc.process_bytes(read_buffer_, crc_location - read_buffer_);
    const uint16_t expected_crc = crc.checksum();

    data.ignore(payload_size);
    const auto maybe_actual_crc = read_stream.Read<uint16_t>();
    MJ_ASSERT(!!maybe_actual_crc);
    const auto actual_crc = *maybe_actual_crc;

    if (expected_crc != actual_crc) {
      // Whoops, we should log this checksum mismatch somewhere.
      // Assume that we are no longer synchronized and start from the
      // next header possibility.
      stats_.checksum_mismatch++;
      Consume(2);
      return false;
    }

    // Great, we got something!
    const auto size_to_write =
        std::min<std::ptrdiff_t>(payload_size, current_read_data_.size());
    std::memcpy(current_read_data_.data(), payload_start, size_to_write);

    current_read_header_->source = *maybe_source_id;
    current_read_header_->destination = *maybe_dest_id;
    current_read_header_->size = payload_size;

    Consume(crc_location - read_buffer_ + 2);

    InvokeReadCallback(
        (static_cast<std::ptrdiff_t>(payload_size) > size_to_write) ?
        micro::error_code(errc::kPayloadTruncated) :
        micro::error_code(),
        size_to_write);

    return true;
  }

  void Consume(std::streamsize size) {
    if (size == 0) { return; }
    MJ_ASSERT(read_start_ >= size);
    std::memmove(read_buffer_, &read_buffer_[size], read_start_ - size);
    read_start_ -= size;
  }

  void InvokeReadCallback(const micro::error_code& ec, std::ptrdiff_t size) {
    // We do a little dance here in case invoking the callback causes
    // another read to be made (which is almost a certainty).
    auto copy = current_read_callback_;
    current_read_callback_ = {};
    current_read_header_ = nullptr;
    current_read_data_ = {};

    copy(ec, size);
  }

  micro::AsyncStream* const stream_;
  const Options options_;

  std::streamsize read_start_ = 0;
  char* const read_buffer_ = {};
  char* const write_buffer_ = {};

  micro::AsyncWriter async_writer_;

  micro::SizeCallback current_read_callback_;
  Header* current_read_header_ = nullptr;
  base::string_span current_read_data_;

  micro::SizeCallback current_write_callback_;
  std::size_t current_write_size_ = 0;

  Stats stats_;
};

MicroStreamDatagram::MicroStreamDatagram(
    micro::Pool* pool, micro::AsyncStream* stream, const Options& options)
    : impl_(pool, pool, stream, options) {}

MicroStreamDatagram::~MicroStreamDatagram() {}

void MicroStreamDatagram::AsyncRead(Header* header,
                                    const base::string_span& data,
                                    const micro::SizeCallback& callback) {
  impl_->AsyncRead(header, data, callback);
}

void MicroStreamDatagram::AsyncWrite(const Header& header,
                                     const std::string_view& data,
                                     const micro::SizeCallback& callback) {
  impl_->AsyncWrite(header, data, callback);
}

MicroDatagramServer::Properties MicroStreamDatagram::properties() const {
  Properties result;
  result.max_size = 115;
  return result;
}

const MicroStreamDatagram::Stats* MicroStreamDatagram::stats() const {
  return &impl_->stats_;
}

}
}
