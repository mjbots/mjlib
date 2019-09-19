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
#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/stream.h"

namespace mjlib {
namespace multiplex {

template <typename Base = base::ReadStream>
class ReadStream {
 public:
  ReadStream(Base& istr) : istr_(istr) {}
  Base* base() { return &istr_; }

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
#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
      return {};
#ifndef __clang__
#pragma GCC diagnostic pop
#endif
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

  Base& istr_;
};

// TODO(jpieper): Figure out how to ensure that callers properly
// manage buffer length.
template <>
inline std::optional<uint32_t>
ReadStream<base::BufferReadStream>::ReadVaruint() {
  uint32_t result = 0;
  const uint8_t* position = reinterpret_cast<const uint8_t*>(istr_.position());
  auto remaining = istr_.remaining();

  int pos = 0;
  int i = 0;
  for (; i < 5; i++) {
    if (remaining == 0) {
      istr_.fast_ignore(i);
      return {};
    }
    remaining--;

    const auto this_byte = *position;
    position++;

    result |= (this_byte & 0x7f) << pos;
    pos += 7;
    if ((this_byte & 0x80) == 0) {
      istr_.fast_ignore(i + 1);
      return result;
    }
  }

  istr_.fast_ignore(i);
  // Whoops, this was too big.  For now, just return maxint.
  return std::numeric_limits<uint32_t>::max();
}

template <>
template <typename T>
inline std::optional<T>
ReadStream<base::BufferReadStream>::ReadScalar() {
  if (istr_.remaining() < static_cast<std::streamsize>(sizeof(T))) {
    return {};
  }
  const T* const position = reinterpret_cast<const T*>(istr_.position());
  istr_.fast_ignore(sizeof(T));
  return *position;
}

template <typename Base = base::WriteStream>
class WriteStream {
 public:
  WriteStream(Base& ostr) : ostr_(ostr) {}
  Base* base() { return &ostr_; }

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

  Base& ostr_;
};

template <>
inline void
WriteStream<base::BufferWriteStream>::WriteVaruint(uint32_t value) {
  uint8_t* start_position = reinterpret_cast<uint8_t*>(ostr_.position());
  uint8_t* position = start_position;

  do {
    uint8_t this_byte = value & 0x7f;
    value >>= 7;
    this_byte |= value ? 0x80 : 0x00;
    *position = this_byte;
    position++;
  } while (value);
  ostr_.skip(position - start_position);
}

template <>
template <typename T>
inline void
WriteStream<base::BufferWriteStream>::WriteScalar(T value) {
  T* position = reinterpret_cast<T*>(ostr_.position());
  *position = value;
  ostr_.skip(sizeof(T));
}

}
}
