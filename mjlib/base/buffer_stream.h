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

#include <string_view>

#include "mjlib/base/assert.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/string_span.h"

namespace mjlib {
namespace base {

class BufferWriteStream : public WriteStream {
 public:
  BufferWriteStream(const string_span& buffer) : buffer_(buffer) {}

  void write(const std::string_view& data) override {
    MJ_ASSERT(static_cast<std::streamsize>(offset_ + data.size()) <=
              buffer_.size());
    std::memcpy(&buffer_[offset_], data.data(), data.size());
    offset_ += data.size();
  }

  void skip(std::streamsize amount) {
    // MJ_ASSERT((offset_ + amount) <=
    //           static_cast<std::streamsize>(buffer_.size()));
    offset_ += amount;
  }

  void reset(char* position) {
    MJ_ASSERT(position >= &buffer_[0] &&
              position < &buffer_[buffer_.size()]);
    offset_ = position - &buffer_[0];
  }

  std::streamsize offset() const noexcept { return offset_; }
  std::streamsize remaining() const noexcept { return buffer_.size() - offset_; }
  std::streamsize size() const noexcept { return buffer_.size(); }
  char* position() { return &buffer_[offset_]; }

 private:
  const string_span buffer_;
  std::streamsize offset_ = 0;
};

class BufferReadStream : public ReadStream {
 public:
  BufferReadStream(const std::string_view& buffer) : buffer_(buffer) {}

  void ignore(std::streamsize amount) override {
    std::streamsize to_ignore =
        std::min<std::streamsize>(amount, buffer_.size() - offset_);
    fast_ignore(to_ignore);
  }

  void fast_ignore(std::streamsize to_ignore) {
    last_read_ = to_ignore;
    offset_ += last_read_;
  }

  void read(const string_span& buffer) override {
    std::streamsize to_read =
        std::min<std::streamsize>(buffer.size(), buffer_.size() - offset_);
    last_read_ = to_read;
    std::memcpy(buffer.data(), &buffer_[offset_], last_read_);
    offset_ += last_read_;
  }

  std::streamsize gcount() const override { return last_read_; }

  std::streamsize offset() const noexcept { return offset_; }
  const char* position() const noexcept { return &buffer_[offset_]; }
  std::streamsize remaining() const noexcept { return buffer_.size() - offset_; }
  std::streamsize size() const noexcept { return buffer_.size(); }

 private:
  const std::string_view buffer_;
  std::streamsize offset_ = 0;
  std::streamsize last_read_ = 0;
};

}
}
