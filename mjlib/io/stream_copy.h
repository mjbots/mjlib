// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#pragma once

#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>

#include "mjlib/base/fail.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

class StreamCopy {
 public:
  StreamCopy(const boost::asio::executor& executor,
             AsyncReadStream* read_stream, AsyncWriteStream* write_stream,
             ErrorCallback done_callback)
      : executor_(executor),
        read_stream_(read_stream),
        write_stream_(write_stream),
        done_callback_(done_callback) {
    StartRead();
  }

 private:
  void StartRead() {
    read_stream_->async_read_some(
        boost::asio::buffer(buffer_),
        std::bind(&StreamCopy::HandleRead, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleRead(const base::error_code& ec, size_t size) {
    if (ec) {
      if (done_callback_) {
        boost::asio::post(
            executor_,
            std::bind(done_callback_, ec));
        done_callback_ = {};
      }
      return;
    }
    boost::asio::async_write(
        *write_stream_,
        boost::asio::buffer(buffer_, size),
        std::bind(&StreamCopy::HandleWrite, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleWrite(const base::error_code& ec, size_t) {
    if (ec) {
      if (done_callback_) {
        boost::asio::post(
            executor_,
            std::bind(done_callback_, ec));
        done_callback_ = {};
      }
      return;
    }
    StartRead();
  }

  boost::asio::executor executor_;
  AsyncReadStream* const read_stream_;
  AsyncWriteStream* const write_stream_;
  char buffer_[4096] = {};
  ErrorCallback done_callback_;
};

class BidirectionalStreamCopy {
 public:
  BidirectionalStreamCopy(const boost::asio::executor& executor,
                          AsyncStream* left, AsyncStream* right,
                          ErrorCallback done_callback)
      : executor_(executor),
        copy1_(executor, left, right, std::bind(
                   &BidirectionalStreamCopy::HandleDone, this,
                   std::placeholders::_1)),
        copy2_(executor, right, left, std::bind(
                   &BidirectionalStreamCopy::HandleDone, this,
                   std::placeholders::_1)),
        done_callback_(done_callback) {}

 private:
  void HandleDone(const base::error_code& ec) {
    if (!done_callback_) { return; }

    boost::asio::post(
        executor_,
        std::bind(done_callback_, ec));
    done_callback_ = {};
  }

  boost::asio::executor executor_;
  StreamCopy copy1_;
  StreamCopy copy2_;
  ErrorCallback done_callback_;
};

}
}
