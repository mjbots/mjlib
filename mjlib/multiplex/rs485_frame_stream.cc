// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/rs485_frame_stream.h"

#include <functional>

#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/crc.hpp>

#include "mjlib/base/crc_stream.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/io/deadline_timer.h"
#include "mjlib/io/streambuf_read_stream.h"
#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/stream.h"

namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {

namespace {
constexpr size_t kBlockSize = 4096;
}

class Rs485FrameStream::Impl {
 public:
  Impl(io::AsyncStream* stream) : stream_(stream) {}

  void AsyncWrite(const Frame* frame, io::ErrorCallback callback) {
    write_buffer_.data()->clear();

    frame->encode(&write_buffer_);

    boost::asio::async_write(
        *stream_,
        boost::asio::buffer(write_buffer_.view()),
        [callback = std::move(callback)](auto&& ec, auto&&) mutable {
          callback(ec);
        });
  }

  void AsyncWriteMultiple(const std::vector<const Frame*>& frames,
                          io::ErrorCallback callback) {
    write_buffer_.data()->clear();

    for (auto& frame : frames) {
      frame->encode(&write_buffer_);
    }
    boost::asio::async_write(
        *stream_,
        boost::asio::buffer(write_buffer_.view()),
        [callback = std::move(callback)](auto&& ec, auto&&) mutable {
          callback(ec);
        });
  }

  void AsyncRead(Frame* frame,
                 boost::posix_time::time_duration timeout,
                 io::ErrorCallback callback) {
    BOOST_ASSERT(current_frame_ == nullptr);
    BOOST_ASSERT(!current_callback_);

    current_frame_ = frame;
    current_callback_ = std::move(callback);

    if (timeout == boost::posix_time::time_duration()) {
      timer_.cancel();
    } else {
      timer_.expires_from_now(timeout);
      timer_.async_wait(std::bind(&Impl::HandleTimer, this, pl::_1));
    }

    MaybeStartRead();
  }

  void cancel() {
    if (current_callback_) {
      auto ec = base::error_code(boost::asio::error::operation_aborted);
      boost::asio::post(
          stream_->get_executor(),
          std::bind(std::move(current_callback_), ec));
      current_callback_ = {};
      current_frame_ = nullptr;
    }
    // We won't bother canceling writes, as they are unlikely to take
    // long anyways.
  }

  bool read_data_queued() const {
    return streambuf_.size() > 0;
  }

  boost::asio::any_io_executor get_executor() const {
    return stream_->get_executor();
  }

 private:
  void MaybeStartRead() {
    if (read_outstanding_) { return; }
    if (!current_callback_) { return; }

    // Try parsing what we have first.
    ParseFrame();

    // If that satisfied us, then we're done and don't have to read
    // more.
    if (!current_callback_) { return; }

    read_outstanding_ = true;
    stream_->async_read_some(
        streambuf_.prepare(kBlockSize),
        std::bind(&Impl::HandleRead, this, pl::_1, pl::_2));
  }

  void HandleRead(const base::error_code& ec, size_t size) {
    base::FailIf(ec);

    streambuf_.commit(size);
    read_outstanding_ = false;

    if (current_callback_) {
      ParseFrame();
    }

    MaybeStartRead();
  }

  void ParseFrame() {
    BOOST_ASSERT(current_frame_);

    while (true) {
      // Try to find a complete frame in our input buffer, discarding
      // anything that doesn't look like a frame.
      DiscardUntil(Format::kHeader & 0xff);

      io::StreambufReadStream stream{&streambuf_};
      base::CrcReadStream<boost::crc_ccitt_type> crc_stream{stream};
      ReadStream reader{crc_stream};

      auto maybe_header = reader.Read<uint16_t>();
      if (!maybe_header) { return; }
      if (*maybe_header != Format::kHeader) {
        // Discard two bytes and try again.
        streambuf_.consume(2);
        continue;
      }

      auto maybe_source_id = reader.Read<uint8_t>();
      if (!maybe_source_id) { return; }
      current_frame_->source_id = *maybe_source_id & 0x7f;
      current_frame_->request_reply = (*maybe_source_id & 0x80) != 0;

      auto maybe_dest_id = reader.Read<uint8_t>();
      if (!maybe_dest_id) { return; }
      current_frame_->dest_id = *maybe_dest_id;

      auto maybe_payload_size = reader.ReadVaruint();
      if (!maybe_payload_size) { return; }

      if (stream.remaining() < (*maybe_payload_size + 2)) {
        return;
      }

      current_frame_->payload.resize(*maybe_payload_size);
      crc_stream.read(base::string_span(&current_frame_->payload[0],
                                        *maybe_payload_size));

      const auto calculated_checksum = crc_stream.checksum();
      const auto read_checksum = reader.Read<uint16_t>();

      // We have enough data. Lets see if the checksum matches.
      if (calculated_checksum != *read_checksum) {
        // Nope.  Skip this header and try again.
        streambuf_.consume(2);
        continue;

        // Maybe we should instead report this as an error?
      }

      streambuf_.consume(stream.offset());

      // Woot!  We have a full functioning frame.  Let's report that.
      current_frame_ = nullptr;
      auto copy = std::move(current_callback_);
      current_callback_ = {};

      boost::asio::post(
          stream_->get_executor(),
          std::bind(std::move(copy), base::error_code()));

      return;
    }
  }

  void DiscardUntil(uint8_t value) {
    auto buffers = streambuf_.data();
    const auto begin = iterator::begin(buffers);
    const auto end = iterator::end(buffers);
    const auto it = std::find_if(
        begin, end, [&](char c) { return static_cast<uint8_t>(c) == value; });
    const auto to_discard = it - begin;
    streambuf_.consume(to_discard);
  }

  void HandleTimer(const base::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      // The timer was canceled.
      return;
    }

    base::FailIf(ec);

    if (current_callback_) {
      BOOST_ASSERT(current_frame_);
      current_frame_ = nullptr;
      auto copy = std::move(current_callback_);
      current_callback_ = {};
      copy(boost::asio::error::operation_aborted);
    }
  }

  io::AsyncStream* const stream_;
  base::FastOStringStream write_buffer_;

  bool read_outstanding_ = false;
  boost::asio::streambuf streambuf_;
  using const_buffers_type = boost::asio::streambuf::const_buffers_type;
  using iterator = boost::asio::buffers_iterator<const_buffers_type>;

  io::DeadlineTimer timer_{stream_->get_executor()};

  Frame* current_frame_ = nullptr;
  io::ErrorCallback current_callback_;
};

Rs485FrameStream::Rs485FrameStream(
    const boost::asio::any_io_executor&, const Options&, io::AsyncStream* stream)
    : impl_(std::make_unique<Impl>(stream)) {}
Rs485FrameStream::~Rs485FrameStream() {}

FrameStream::Properties Rs485FrameStream::properties() const {
  Properties properties;
  return properties;
}

void Rs485FrameStream::AsyncStart(io::ErrorCallback callback) {
  boost::asio::post(
      impl_->get_executor(),
      std::bind(std::move(callback), base::error_code()));
}

void Rs485FrameStream::AsyncWrite(
    const Frame* frame, io::ErrorCallback callback) {
  impl_->AsyncWrite(frame, std::move(callback));
}

void Rs485FrameStream::AsyncWriteMultiple(
    const std::vector<const Frame*>& frames, io::ErrorCallback callback) {
  impl_->AsyncWriteMultiple(frames, std::move(callback));
}

void Rs485FrameStream::AsyncRead(
    Frame* frame,
    boost::posix_time::time_duration timeout,
    io::ErrorCallback callback) {
  impl_->AsyncRead(frame, timeout, std::move(callback));
}

void Rs485FrameStream::cancel() {
  impl_->cancel();
}

bool Rs485FrameStream::read_data_queued() const {
  return impl_->read_data_queued();
}

boost::asio::any_io_executor Rs485FrameStream::get_executor() const {
  return impl_->get_executor();
}

}
}
