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

#include <ios>
#include <string_view>

#include "mjlib/base/string_span.h"

namespace mjlib {
namespace base {

class WriteStream {
 public:
  virtual ~WriteStream() {}
  virtual void write(const std::string_view&) = 0;

  struct Iterator {
    Iterator(WriteStream& stream) : stream_(&stream) {}

    using iterator_category = std::output_iterator_tag;
    using value_type = char;
    using difference_type = int;
    using pointer = char*;
    using reference = char&;

    class Proxy {
     public:
      Proxy(WriteStream* stream) : stream_(stream) {}

      Proxy& operator=(char value) {
        stream_->write(std::string_view(&value, 1));
        return *this;
      }

      WriteStream* stream_ = nullptr;
    };


    Proxy operator*() {
      return Proxy(stream_);
    }

    Iterator& operator++() { return *this; }
    Iterator operator++(int) { return *this; };

    WriteStream* stream_ = nullptr;
  };
};

class ReadStream {
 public:
  virtual ~ReadStream() {}
  virtual void ignore(std::streamsize) = 0;
  virtual void read(const string_span&) = 0;
  virtual std::streamsize gcount() const = 0;
};

}
}
