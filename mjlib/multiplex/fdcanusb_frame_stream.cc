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

#include "mjlib/multiplex/fdcanusb_frame_stream.h"

#include <cstddef>
#include <functional>
#include <regex>

#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>

#include <fmt/format.h>

#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/base/tokenizer.h"
#include "mjlib/io/deadline_timer.h"
#include "mjlib/io/streambuf_read_stream.h"
#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/stream.h"

namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {

namespace {
constexpr size_t kBlockSize = 4096;
constexpr std::ptrdiff_t kMaxLineLength = 512;

size_t RoundUpDlc(size_t value) {
  if (value == 0) { return 0; }
  if (value == 1) { return 1; }
  if (value == 2) { return 2; }
  if (value == 3) { return 3; }
  if (value == 4) { return 4; }
  if (value == 5) { return 5; }
  if (value == 6) { return 6; }
  if (value == 7) { return 7; }
  if (value == 8) { return 8; }
  if (value <= 12) { return 12; }
  if (value <= 16) { return 16; }
  if (value <= 20) { return 20; }
  if (value <= 24) { return 24; }
  if (value <= 32) { return 32; }
  if (value <= 48) { return 48; }
  if (value <= 64) { return 64; }
  return 0;
}

void EncodeFdcanusb(const Frame* frame, base::WriteStream* stream) {
  base::WriteStream::Iterator output(*stream);
  fmt::format_to(
      output, "can send {:x} ",
      (frame->source_id | (frame->request_reply ? 0x80 : 0x00)) << 8 |
      (frame->dest_id));
  for (uint8_t c : frame->payload) {
    fmt::format_to(output, "{:02x}", c);
  }
  // FDCAN doesn't allow arbitrary frame sizes.  Thus, we pad to reach
  // an allowable size.
  const auto actual_size = RoundUpDlc(frame->payload.size());
  for (size_t i = frame->payload.size(); i < actual_size; i++) {
    fmt::format_to(output, "50");
  }
  fmt::format_to(output, "\n");
}

int ParseHexNybble(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return -1;
}

int ParseHexByte(const char* value) {
  int high = ParseHexNybble(value[0]);
  if (high < 0) { return high; }
  int low = ParseHexNybble(value[1]);
  if (low < 0) { return low; }
  return (high << 4) | low;
}
}

class FdcanusbFrameStream::Impl {
 public:
  Impl(io::AsyncStream* stream) : stream_(stream) {}

  void AsyncWrite(const Frame* frame, io::ErrorCallback callback) {
    write_buffer_.data()->clear();

    EncodeFdcanusb(frame, &write_buffer_);
    outstanding_oks_++;

    boost::asio::async_write(
        *stream_,
        boost::asio::buffer(write_buffer_.view()),
        [callback = std::move(callback)](
            auto&& ec, auto&&) mutable { callback(ec); });
  }

  void AsyncWriteMultiple(const std::vector<const Frame*>& frames,
                          io::ErrorCallback callback) {
    write_buffer_.data()->clear();

    for (auto& frame : frames) {
      EncodeFdcanusb(frame, &write_buffer_);
      outstanding_oks_++;
    }
    boost::asio::async_write(
        *stream_,
        boost::asio::buffer(write_buffer_.view()),
        [callback = std::move(callback)](
            auto&& ec, auto&&) mutable { callback(ec); });
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
    mjlib::base::Fail("do not call!");
  }

  bool read_data_queued() const {
    if (outstanding_oks_ != 0) { return true; }

    // We need to see if there is a full "rcv" line in our buffer.
    if (streambuf_.size() == 0) { return false; }

    const auto buffers = streambuf_.data();
    auto begin = boost::asio::buffers_begin(buffers);
    auto end = boost::asio::buffers_end(buffers);

    return std::regex_search(begin, end, rcv_present_regex_);
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

    while (current_callback_) {
      if (!ParseFrame()) { break; }
    }

    MaybeStartRead();
  }

  bool ParseFrame() {
    BOOST_ASSERT(current_frame_);

    // Look for a newline.
    auto buffers = streambuf_.data();
    const auto begin = iterator::begin(buffers);
    const auto end = iterator::end(buffers);
    const auto it = std::find_if(
        begin, end, [&](char c) { return c == '\n' || c == '\r'; });
    if (it == end) {
      // No newline present.  See if we've exceeded our maximum line
      // length and can discard.
      if ((it - begin) > kMaxLineLength) {
        streambuf_.consume(it - begin + 1);
      }
      return false;
    }

    // We have a line, try to handle it.
    ParseLine(std::string_view(&*begin, it - begin));
    streambuf_.consume(it - begin + 1);
    return true;
  }

  void ParseLine(std::string_view line) {
    if (line.size() == 0) { return; }

    // We just ignore OKs for now.
    if (line == "OK") {
      outstanding_oks_ = std::max(outstanding_oks_ - 1, 0);
      return;
    }

    if (boost::starts_with(line, "rcv ")) {
      BOOST_ASSERT(current_frame_);
      // This means we have received a frame.  Lets parse it.
      base::Tokenizer tokenizer(line, " ");
      const auto rcv = tokenizer.next();
      const auto address = tokenizer.next();
      const auto data = tokenizer.next();

      if (rcv.size() == 0 || address.size() == 0 || data.size() == 0) {
        // This is malformed.  Ignore for now.
        return;
      }

      const auto int_address =
          std::strtol(address.data(), 0, 16);
      current_frame_->source_id = (int_address >> 8) & 0x7f;
      current_frame_->request_reply = ((int_address >> 8) & 0x80) != 0;
      current_frame_->dest_id = int_address & 0x7f;

      current_frame_->payload.resize(data.size() / 2);
      for (size_t i = 0; i < data.size(); i += 2) {
        current_frame_->payload[i / 2] = ParseHexByte(&data[i]);
      }

      EmitFrame();

      return;
    }
  }

  void EmitFrame() {
    timer_.cancel();

    current_frame_ = nullptr;
    auto copy = std::move(current_callback_);
    current_callback_ = {};

    boost::asio::post(
        get_executor(),
        std::bind(std::move(copy), base::error_code()));
  }

  void HandleTimer(const base::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      // The timer was canceled.
      return;
    }

    base::FailIf(ec);

    // If we hit the timeout, assume no OKs are outstanding.
    outstanding_oks_ = 0;

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
  int outstanding_oks_ = 0;
  boost::asio::streambuf streambuf_;
  using const_buffers_type = boost::asio::streambuf::const_buffers_type;
  using iterator = boost::asio::buffers_iterator<const_buffers_type>;

  io::DeadlineTimer timer_{stream_->get_executor()};

  Frame* current_frame_ = nullptr;
  io::ErrorCallback current_callback_;

  std::regex rcv_present_regex_{"^rcv[[:print:]]+$"};
};

FdcanusbFrameStream::FdcanusbFrameStream(
    const boost::asio::any_io_executor&, const Options&, io::AsyncStream* stream)
    : impl_(std::make_unique<Impl>(stream)) {}
FdcanusbFrameStream::~FdcanusbFrameStream() {}

FrameStream::Properties FdcanusbFrameStream::properties() const {
  Properties properties;
  properties.max_size = 64;
  return properties;
}

void FdcanusbFrameStream::AsyncStart(io::ErrorCallback callback) {
  boost::asio::post(
      impl_->get_executor(),
      std::bind(std::move(callback), base::error_code()));
}

void FdcanusbFrameStream::AsyncWrite(
    const Frame* frame, io::ErrorCallback callback) {
  impl_->AsyncWrite(frame, std::move(callback));
}

void FdcanusbFrameStream::AsyncWriteMultiple(
    const std::vector<const Frame*>& frames, io::ErrorCallback callback) {
  impl_->AsyncWriteMultiple(frames, std::move(callback));
}

void FdcanusbFrameStream::AsyncRead(
    Frame* frame,
    boost::posix_time::time_duration timeout,
    io::ErrorCallback callback) {
  impl_->AsyncRead(frame, timeout, std::move(callback));
}

void FdcanusbFrameStream::cancel() {
  impl_->cancel();
}

bool FdcanusbFrameStream::read_data_queued() const {
  return impl_->read_data_queued();
}

boost::asio::any_io_executor FdcanusbFrameStream::get_executor() const {
  return impl_->get_executor();
}

}
}
