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

#pragma once

#include <limits>
#include <optional>

#include "mjlib/base/assert.h"
#include "mjlib/base/stream.h"

namespace mjlib {
namespace multiplex {

class ReadStream {
 public:
  ReadStream(base::ReadStream& istr) : istr_(istr) {}
  base::ReadStream* base() { return &istr_; }

  template <typename T>
  std::optional<T> Read() {
    return ReadScalar<T>();
  }

  std::optional<uint32_t> ReadVaruint() {
    uint32_t result = 0;
    int pos = 0;
    for (int i = 0; i < 5; i++) {
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

  template <typename T>
  std::optional<T> ReadScalar() {
    T result = T{};
    const bool complete = RawRead(reinterpret_cast<char*>(&result), sizeof(result));
    if (!complete) {
      return std::make_optional<T>();
    }
    return result;
  }

 private:
  bool RawRead(char* out, std::streamsize size) {
    istr_.read({out, size});
    if (istr_.gcount() != size) {
      return false;
    }
    return true;
  }

  base::ReadStream& istr_;
};

class WriteStream {
 public:
  WriteStream(base::WriteStream& ostr) : ostr_(ostr) {}
  base::WriteStream* base() { return &ostr_; }

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
    ostr_.write({reinterpret_cast<const char*>(&value), sizeof(value)});
  }

  base::WriteStream& ostr_;
};

}
}
