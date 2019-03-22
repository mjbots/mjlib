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

#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/streambuf.hpp>

#include "mjlib/base/stream.h"

namespace mjlib {
namespace io {

/// Implements a base::ReadStream over the contents of a
/// boost::asio::streambuf.
class StreambufReadStream : public base::ReadStream {
 public:
  StreambufReadStream(boost::asio::streambuf* buffer) : buffer_(buffer) {}

  void ignore(std::streamsize size) override {
    const auto to_ignore = std::min(size, remaining());
    current_ = std::next(current_, to_ignore);
    last_read_ = to_ignore;
  }

  void read(const base::string_span& data) override {
    const auto to_read = std::min<std::streamsize>(data.size(), remaining());
    boost::asio::buffer_copy(
        boost::asio::buffer(data.data(), to_read),
        buffers_ + (current_ - begin_));
    current_ = std::next(current_, to_read);
    last_read_ = to_read;
  }

  std::streamsize gcount() const override {
    return last_read_;
  }

  std::streamsize offset() const {
    return current_ - begin_;
  }

  std::streamsize remaining() const {
    return end_ - current_;
  }

 private:
  using const_buffers_type = boost::asio::streambuf::const_buffers_type;
  using iterator = boost::asio::buffers_iterator<const_buffers_type>;

  boost::asio::streambuf* const buffer_;
  const_buffers_type buffers_{buffer_->data()};
  iterator begin_{iterator::begin(buffers_)};
  iterator end_{iterator::end(buffers_)};
  iterator current_{begin_};

  std::streamsize last_read_ = 0;
};

}
}
