// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include <functional>
#include <string>

#include "mjlib/base/fail.h"
#include "mjlib/io/async_stream.h"

namespace mjlib {
namespace io {
namespace test {

/// Continuously reads from an AsyncStream.  Useful for unit tests.
class Reader {
 public:
  Reader(AsyncReadStream* stream) : stream_(stream) {
    StartRead();
  }

  const std::string& data() const { return data_; }
  void data(const std::string& value) { data_ = value; }
  void clear() { data_ = {}; }

 private:
  void StartRead() {
    stream_->async_read_some(
        boost::asio::buffer(buffer_),
        std::bind(&Reader::HandleRead, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleRead(const base::error_code& ec, size_t size) {
    base::FailIf(ec);
    data_ += std::string(buffer_, size);
    StartRead();
  }

  AsyncReadStream* const stream_;
  char buffer_[256] = {};
  std::string data_;
};

}
}
}
