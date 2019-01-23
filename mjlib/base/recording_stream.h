// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include <vector>

#include "mjlib/base/fast_stream.h"
#include "mjlib/base/stream.h"

namespace mjlib {
namespace base {

class RecordingStream : public ReadStream {
 public:
  RecordingStream(ReadStream& istr) : istr_(istr) {}

  void read(const string_span& buffer) override {
    istr_.read(buffer);
    std::streamsize result = istr_.gcount();
    ostr_.write(std::string_view(buffer.data(), result));
  }

  void ignore(std::streamsize size) override {
    if (static_cast<std::streamsize>(ignore_buffer_.size()) < size) {
      ignore_buffer_.resize(size);
    }
    istr_.read({&ignore_buffer_[0], size});
    ostr_.write(std::string_view(&ignore_buffer_[0], istr_.gcount()));
  }

  std::streamsize gcount() const override {
    return istr_.gcount();
  }

  std::string str() const { return ostr_.str(); }

  ReadStream& istr_;
  FastOStringStream ostr_;
  std::vector<char> ignore_buffer_;
};
}
}
