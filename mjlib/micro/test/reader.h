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

#pragma once

#include <sstream>

#include "mjlib/base/system_error.h"

#include "mjlib/micro/async_stream.h"

namespace mjlib {
namespace micro {
namespace test {

class Reader {
 public:
  Reader(AsyncReadStream* stream) : stream_(stream) {
    StartRead();
  }

  void StartRead() {
    stream_->AsyncReadSome(
        base::string_span(buffer_, buffer_ + sizeof(buffer_)),
        [this](base::error_code ec, ssize_t size) {
          BOOST_TEST(!ec);
          data_.write(buffer_, size);
          this->StartRead();
        });
  }

  AsyncReadStream* const stream_;
  std::ostringstream data_;
  char buffer_[10] = {};
};

}
}
}
