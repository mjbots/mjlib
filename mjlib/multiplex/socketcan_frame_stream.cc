// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/socketcan_frame_stream.h"

#ifndef _WIN32
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#endif  // _WIN32

#include <cstddef>
#include <functional>
#include <regex>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>

#include <fmt/format.h>

#include "mjlib/base/assert.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/base/system_error.h"
#include "mjlib/io/deadline_timer.h"

namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {

#ifndef _WIN32
namespace {

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
}

class SocketcanFrameStream::Impl {
 public:
  Impl(const boost::asio::any_io_executor& executor, const Options& options)
      : options_(options),
        executor_(executor) {
    socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);

    struct ifreq ifr = {};
    ::strncpy(&ifr.ifr_name[0], options.interface.c_str(), sizeof(ifr.ifr_name));
    base::system_error::throw_if(
        ::ioctl(socket_, SIOCGIFINDEX, &ifr) < 0);

    int enable_canfd = 1;
    base::system_error::throw_if(
        ::setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                     &enable_canfd, sizeof(enable_canfd)) != 0);

    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    base::system_error::throw_if(
        ::bind(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0);

    stream_.assign(socket_);
  }

  void AsyncWrite(const Frame* frame, io::ErrorCallback callback) {
    send_frame_.can_id =
        ((frame->source_id | (frame->request_reply ? 0x80 : 0x00)) << 8) |
        frame->dest_id;
    if (send_frame_.can_id > 0x7ff) {
      send_frame_.can_id |= CAN_EFF_FLAG;
    }

    const auto actual_size = RoundUpDlc(frame->payload.size());
    send_frame_.len = actual_size;
    std::memcpy(&send_frame_.data[0], &frame->payload[0], frame->payload.size());
    for (size_t i = frame->payload.size(); i < actual_size; i++) {
      send_frame_.data[i] = 0x50;
    }

    stream_.async_write_some(
        boost::asio::buffer(&send_frame_, sizeof(send_frame_)),
        [callback = std::move(callback)](
            auto&& ec, auto&&) mutable { callback(ec); });
  }

  struct WriteMultipleContext {
    const std::vector<const Frame*>* frames = nullptr;
    size_t index = 0;

    io::ErrorCallback callback;
  };

  void AsyncWriteMultiple(const std::vector<const Frame*>& frames,
                          io::ErrorCallback callback) {
    auto ctx = std::make_shared<WriteMultipleContext>();
    ctx->frames = &frames;
    ctx->callback = std::move(callback);

    StartWriteMultipleContext(ctx);
  }

  void StartWriteMultipleContext(std::shared_ptr<WriteMultipleContext> ctx) {
    if (ctx->index >= ctx->frames->size()) {
      // We're done.
      boost::asio::post(
          executor_,
          std::bind(std::move(ctx->callback), base::error_code()));
      return;
    }

    auto this_index = ctx->index;
    ctx->index++;
    AsyncWrite((*ctx->frames)[this_index],
               [ctx, this](const base::error_code& ec) {
                 if (ec) {
                   // an error, bail early
                   boost::asio::post(
                       executor_,
                       std::bind(std::move(ctx->callback), ec));
                   return;
                 }
                 this->StartWriteMultipleContext(ctx);
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

    stream_.async_read_some(
        boost::asio::buffer(&recv_frame_, sizeof(recv_frame_)),
        std::bind(&Impl::HandleRead, this, pl::_1, pl::_2));
  }

  void cancel() {
    mjlib::base::Fail("do not call!");
  }

  bool read_data_queued() const {
    return false;
  }

  boost::asio::any_io_executor get_executor() const {
    return executor_;
  }

 private:
  void HandleRead(const base::error_code& ec, size_t size) {
    auto copy = std::move(current_callback_);
    current_callback_ = {};

    if (ec) {
      current_frame_ = {};
      boost::asio::post(
          executor_,
          std::bind(std::move(copy), ec));

      return;
    }

    // We only handle CAN-FD frames for now.
    MJ_ASSERT(size == CANFD_MTU);

    auto* frame = current_frame_;
    current_frame_ = nullptr;

    frame->source_id = (recv_frame_.can_id & 0x7f00) >> 8;
    frame->dest_id = (recv_frame_.can_id & 0x007f);
    frame->request_reply = (recv_frame_.can_id & 0x8000) != 0;
    frame->payload = std::string(reinterpret_cast<const char*>(&recv_frame_.data[0]),
                                 recv_frame_.len);

    boost::asio::post(
        executor_,
        std::bind(std::move(copy), base::error_code()));
  }

  void HandleTimer(const base::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      // The timer was canceled.
      return;
    }

    base::FailIf(ec);

    if (current_callback_) {
      stream_.cancel();
    }
  }

  const Options options_;
  boost::asio::any_io_executor executor_;

  int socket_ = -1;
  boost::asio::posix::stream_descriptor stream_{executor_};

  io::DeadlineTimer timer_{executor_};

  Frame* current_frame_ = nullptr;
  mjlib::io::ErrorCallback current_callback_;

  struct canfd_frame send_frame_ = {};
  struct canfd_frame recv_frame_ = {};

};

#else // _WIN32

class SocketcanFrameStream::Impl {
 public:
  Impl(const boost::asio::any_io_executor&, const Options&) {}

  void AsyncWrite(const Frame* frame, io::ErrorCallback callback) {
    base::Fail("not supported");
  }

  void AsyncWriteMultiple(const std::vector<const Frame*>&,
                          io::ErrorCallback) {
    base::Fail("not supported");
  }

  void AsyncRead(Frame*,
                 boost::posix_time::time_duration,
                 io::ErrorCallback) {
    base::Fail("not supported");
  }

  void cancel() {
    base::Fail("not supported");
  }

  bool read_data_queued() const {
    return false;
  }

  boost::asio::any_io_executor get_executor() const {
    base::Fail("not supported");
  }
};

#endif

SocketcanFrameStream::SocketcanFrameStream(
    const boost::asio::any_io_executor& executor, const Options& options)
    : impl_(std::make_unique<Impl>(executor, options)) {}
SocketcanFrameStream::~SocketcanFrameStream() {}

FrameStream::Properties SocketcanFrameStream::properties() const {
  Properties properties;
  properties.max_size = 64;
  return properties;
}

void SocketcanFrameStream::AsyncStart(io::ErrorCallback callback) {
  boost::asio::post(
      impl_->get_executor(),
      std::bind(std::move(callback), base::error_code()));
}

void SocketcanFrameStream::AsyncWrite(
    const Frame* frame, io::ErrorCallback callback) {
  impl_->AsyncWrite(frame, std::move(callback));
}

void SocketcanFrameStream::AsyncWriteMultiple(
    const std::vector<const Frame*>& frames, io::ErrorCallback callback) {
  impl_->AsyncWriteMultiple(frames, std::move(callback));
}

void SocketcanFrameStream::AsyncRead(
    Frame* frame,
    boost::posix_time::time_duration timeout,
    io::ErrorCallback callback) {
  impl_->AsyncRead(frame, timeout, std::move(callback));
}

void SocketcanFrameStream::cancel() {
  impl_->cancel();
}

bool SocketcanFrameStream::read_data_queued() const {
  return impl_->read_data_queued();
}

boost::asio::any_io_executor SocketcanFrameStream::get_executor() const {
  return impl_->get_executor();
}

}
}
