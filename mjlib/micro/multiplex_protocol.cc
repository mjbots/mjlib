// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/multiplex_protocol.h"

#include <functional>

#include <boost/crc.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/visitor.h"

namespace mjlib {
namespace micro {

namespace {
int GetVaruintSize(uint32_t value) {
  int result = 0;
  do {
    value >>= 7;
    result++;
  } while (value);
  return result;
}

class ProtocolReadStream {
 public:
  ProtocolReadStream(base::BufferReadStream& istr) : istr_(istr) {}
  base::BufferReadStream* base() { return &istr_; }

  template <typename T>
  std::optional<T> Read() {
    return ReadScalar<T>();
  }

  std::optional<uint32_t> ReadVaruint() {
    uint32_t result = 0;
    int pos = 0;
    for (int i = 0; i < 5; i++) {
      if (istr_.remaining() == 0) { return {}; }
      const auto maybe_this_byte = Read<uint8_t>();
      if (!maybe_this_byte) {
        return {};
      }
      const auto this_byte = *maybe_this_byte;
      result |= (this_byte & 0x7f) << pos;
      pos += 7;
      if ((this_byte & 0x80) == 0) { return result; }
    }
    // Whoops, this was too big.  For now, just return maxint.
    return std::numeric_limits<uint32_t>::max();
  }

 private:
  template <typename T>
  std::optional<T> ReadScalar() {
    if (istr_.remaining() < static_cast<std::streamsize>(sizeof(T))) {
      return {};
    }

    T result = T{};
    RawRead(reinterpret_cast<char*>(&result), sizeof(result));
    return result;
  }

  void RawRead(char* out, std::streamsize size) {
    istr_.read({out, size});
    MJ_ASSERT(istr_.gcount() == size);
  }

  base::BufferReadStream& istr_;
};

class ProtocolWriteStream {
 public:
  ProtocolWriteStream(base::BufferWriteStream& ostr) : ostr_(ostr) {}
  base::BufferWriteStream* base() { return &ostr_; }

  template <typename T>
  void Write(T value) {
    WriteScalar<T>(value);
  }

  void WriteVaruint(uint32_t value) {
    do {
      uint8_t this_byte = value & 0x7f;
      value >>= 7;
      this_byte |= value ? 0x80 : 0x00;
      Write(this_byte);
    } while (value);
  }

 private:
  template <typename T>
  void WriteScalar(T value) {
    MJ_ASSERT(ostr_.remaining() >= static_cast<std::streamsize>(sizeof(T)));

    ostr_.write({reinterpret_cast<const char*>(&value), sizeof(value)});
  }

  base::BufferWriteStream& ostr_;
};

}

class MultiplexProtocolServer::Impl {
 public:
  class TunnelStream : public AsyncStream {
   public:
    void set_parent(Impl* impl) { parent_ = impl; }
    uint32_t id() const { return id_; }
    void set_id(uint32_t id) { id_ = id; }

    void AsyncReadSome(const base::string_span& buffer,
                       const SizeCallback& callback) override {
      MJ_ASSERT(read_buffer_.empty());
      read_buffer_ = buffer;
      read_callback_ = callback;

      // TODO: Return immediately for size 0 reads (may need an event
      // queue).

      DoReadTransfer();
    }

    void AsyncWriteSome(const std::string_view& buffer,
                        const SizeCallback& callback) override {
      MJ_ASSERT(write_buffer_.empty());
      write_buffer_ = buffer;
      write_callback_ = callback;

      // TODO: Return immediately for size 0 writes (may need an event
      // queue).
    }

    void DoReadTransfer() {
      if (read_buffer_.empty() ||
          read_data_size_ == 0) {
        return;
      }

      const auto to_copy = std::min<std::streamsize>(
          read_buffer_.size(), read_data_size_);
      std::memcpy(read_buffer_.data(), read_data_, to_copy);
      std::memmove(read_data_, &read_data_[to_copy], read_data_size_ - to_copy);
      read_data_size_ -= to_copy;

      read_buffer_ = {};

      auto callback = read_callback_;
      read_callback_ = {};

      callback({}, to_copy);
    }

    Impl* parent_ = nullptr;
    uint32_t id_ = 0;

    base::string_span read_buffer_;
    SizeCallback read_callback_;

    char read_data_[128] = {};
    ssize_t read_data_size_ = 0;

    std::string_view write_buffer_;
    SizeCallback write_callback_;
  };

  Impl(Pool* pool,
       AsyncStream* stream,
       Server*,
       const Options& options)
      : options_(options),
        stream_(stream),
        read_buffer_(static_cast<char*>(
                         pool->Allocate(options.buffer_size, 1))),
        write_buffer_(static_cast<char*>(
                          pool->Allocate(options.buffer_size, 1))) {
    for (auto& tunnel : tunnels_) {
      tunnel.set_parent(this);
    }
  }

  AsyncStream* MakeTunnel(uint32_t id) {
    MJ_ASSERT(id != 0);
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == 0) {
        // This one is unallocated.
        tunnel.set_id(id);
        return &tunnel;
      }
    }

    MJ_ASSERT(false);
    return nullptr;
  }

  void Start() {
    MaybeStartReadFrame();
  }

  const Stats* stats() const { return &stats_; }
  Config* config() { return &config_; }

 private:
  void MaybeStartReadFrame() {
    if (read_outstanding_) { return; }

    read_outstanding_ = true;
    stream_->AsyncReadSome(
        base::string_span(&read_buffer_[read_start_],
                          read_buffer_ + options_.buffer_size),
        std::bind(&Impl::HandleReadFrame, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleReadFrame(const base::error_code& ec, size_t size) {
    read_outstanding_ = false;

    if (ec) {
      // We don't really have a way to log or do anything here.  So
      // lets just bail and start over.
      read_start_ = 0;
      MaybeStartReadFrame();
      return;
    }

    // Advance read_start_ to where our next invalid byte is.
    read_start_ += size;

    for (;;) {
      if (!HandleMaybeFrame()) {
        break;
      }
    }

    MaybeStartReadFrame();
  }

  // Return true if more data might be available in the buffer.
  bool HandleMaybeFrame() {
    // For now, we'll require that we fit a whole valid packet into
    // our buffer at once before acting on it.  So we'll just keep
    // calling StartReadFrame until we get there.

    // Work to start our buffer out with the frame header.

    char* const found = static_cast<char*>(
        std::memchr(read_buffer_, (kHeader & 0xff), read_start_));
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
    ProtocolReadStream read_stream{data};

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
    const char* const payload_start = read_stream.base()->position();
    const auto data_we_have =
        &read_buffer_[read_start_] - read_stream.base()->position();
    if (data_we_have < static_cast<ssize_t>(payload_size + 2)) {
      // We need more still.
      return false;
    }

    const char* const crc_location =
        read_stream.base()->position() + payload_size;

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
      return true;
    }

    if (*maybe_dest_id != config_.id) {
      stats_.wrong_id++;
      Consume(read_stream.base()->position() - read_buffer_);
      return true;
    }

    base::BufferWriteStream buffer_write_stream{
      base::string_span(write_buffer_, options_.buffer_size)};
    ProtocolWriteStream write_stream{buffer_write_stream};
    const bool need_response =
        ((*maybe_source_id) & 0x80) != 0 &&
        !write_outstanding_;

    // Everything checked out.  Now we we can process our subframes.
    ProcessSubframes(
        std::string_view(payload_start, payload_size),
        need_response ? &write_stream : nullptr);
    const auto to_consume = crc_location - read_buffer_ + 2;
    Consume(to_consume);

    if (need_response) {
      WriteResponse(*maybe_source_id & 0x7f, buffer_write_stream.offset());
    }

    return true;
  }

  void Consume(std::streamsize size) {
    MJ_ASSERT(read_start_ >= size);
    if (size == 0) { return; }
    std::memmove(read_buffer_, &read_buffer_[size], read_start_ - size);
    read_start_ -= size;
  }

  void WriteResponse(uint8_t client_id, std::streamsize response_size) {
    MJ_ASSERT(!write_outstanding_);

    // First, figure out how big our size varuint will end up being.
    const auto header_size =
        2 + // kHeader
        1 + // source id
        1 + // dest_id
        GetVaruintSize(response_size);
    std::memmove(&write_buffer_[header_size], &write_buffer_[0], response_size);

    {
      base::BufferWriteStream header_buffer_stream(
          base::string_span(write_buffer_, header_size));
      ProtocolWriteStream header_stream(header_buffer_stream);
      header_stream.Write(kHeader);
      header_stream.Write(config_.id);
      header_stream.Write(client_id);
      header_stream.WriteVaruint(response_size);
    }

    // Now figure out the checksum.
    const auto crc_location = header_size + response_size;
    boost::crc_ccitt_type crc;
    crc.process_bytes(write_buffer_, crc_location);
    const uint16_t actual_crc = crc.checksum();

    {
      base::BufferWriteStream crc_buffer_stream(
          base::string_span(&write_buffer_[crc_location], 2));
      ProtocolWriteStream crc_stream(crc_buffer_stream);
      crc_stream.Write(actual_crc);
    }

    write_outstanding_ = true;
    AsyncWrite(*stream_, std::string_view(write_buffer_, crc_location + 2),
               std::bind(&Impl::HandleWrite, this, std::placeholders::_1));
  }

  void HandleWrite(base::error_code) {
    write_outstanding_ = false;
  }

  void ProcessSubframes(const std::string_view& subframes,
                        ProtocolWriteStream* response_stream) {
    base::BufferReadStream buffer_stream(subframes);
    ProtocolReadStream str(buffer_stream);

    while (buffer_stream.remaining()) {
      const auto maybe_subframe_type = str.ReadVaruint();
      if (!maybe_subframe_type) {
        // The final subframe was malformed.  Guess we'll just ignore it.
        return;
      }

      const auto subframe_type = *maybe_subframe_type;

      if (subframe_type == static_cast<uint8_t>(Subframe::kClientToServer)) {
        // The client sent us some data.
        ProcessSubframeClientToServer(str, response_stream);
      } else {
        // An unknown subframe.  Write off the rest of this frame as
        // unusable.
        return;
      }
    }
  }

  void ProcessSubframeClientToServer(
      ProtocolReadStream& str, ProtocolWriteStream* response_stream) {
    const auto maybe_channel = str.ReadVaruint();
    const auto maybe_bytes = str.ReadVaruint();
    if (!maybe_channel || !maybe_bytes ||
        str.base()->remaining() < static_cast<std::streamsize>(*maybe_bytes)) {
      // Malformed.
      return;
    }

    auto maybe_tunnel = FindTunnel(*maybe_channel);
    if (!maybe_tunnel) {
      return;
    }

    auto& tunnel = *maybe_tunnel;

    const auto remaining_space =
        sizeof(tunnel.read_data_) - tunnel.read_data_size_;

    if (*maybe_bytes > remaining_space) {
      stats_.receive_overrun++;
    } else {
      str.base()->read({&tunnel.read_data_[tunnel.read_data_size_],
              static_cast<ssize_t>(*maybe_bytes)});
      tunnel.read_data_size_ += *maybe_bytes;
    }

    tunnel.DoReadTransfer();

    // Send our response if necessary.
    if (response_stream) {
      response_stream->Write(Subframe::kServerToClient);
      response_stream->WriteVaruint(*maybe_channel);

      const ssize_t kExtraPadding = 16;
      const auto to_copy =
          std::min<std::streamsize>(
              tunnel.write_buffer_.size(),
              response_stream->base()->remaining() -
              (kMaxVaruintSize + kCrcSize + kExtraPadding));
      response_stream->WriteVaruint(to_copy);
      if (to_copy) {
        response_stream->base()->write(tunnel.write_buffer_.substr(0, to_copy));

        tunnel.write_buffer_ = {};
        auto cbk = tunnel.write_callback_;
        tunnel.write_callback_ = {};
        cbk({}, to_copy);
      }
    }
  }

  TunnelStream* FindTunnel(uint32_t id) {
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == id) { return &tunnel; }
    }
    return nullptr;
  }

  const Options options_;
  AsyncStream* const stream_;

  Config config_;

  std::streamsize read_start_ = 0;
  char* const read_buffer_ = {};
  bool read_outstanding_ = false;

  char* const write_buffer_ = {};
  bool write_outstanding_ = false;

  TunnelStream tunnels_[1];
  Stats stats_;
};

MultiplexProtocolServer::MultiplexProtocolServer(
    Pool* pool,
    AsyncStream* async_stream,
    Server* server,
    const Options& options)
    : impl_(pool, pool, async_stream, server, options) {}

MultiplexProtocolServer::~MultiplexProtocolServer() {}

AsyncStream* MultiplexProtocolServer::MakeTunnel(uint32_t id) {
  return impl_->MakeTunnel(id);
}

void MultiplexProtocolServer::Start() {
  impl_->Start();
}

const MultiplexProtocolServer::Stats* MultiplexProtocolServer::stats() const {
  return impl_->stats();
}

MultiplexProtocolServer::Config* MultiplexProtocolServer::config() {
  return impl_->config();
}

}
}
