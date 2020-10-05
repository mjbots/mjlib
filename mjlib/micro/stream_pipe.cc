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

#include "mjlib/micro/stream_pipe.h"

#include <cstddef>

#include "mjlib/base/assert.h"

namespace mjlib {
namespace micro {

void StreamPipe::Side::AsyncReadSome(
    const base::string_span& buffer,
    const SizeCallback& callback) {
  // Does our other side have an outstanding write?  If so, satisfy
  // it.
  if (other_->outstanding_write_buffer_.size()) {
    auto to_copy = std::min<std::ptrdiff_t>(
        buffer.size(), other_->outstanding_write_buffer_.size());
    std::memcpy(buffer.data(), other_->outstanding_write_buffer_.data(),
                to_copy);
    pending_read_callback_ = callback;

    parent_->poster_([this, to_copy]() {
        auto read_cbk = this->pending_read_callback_;
        this->pending_read_callback_ = {};

        auto write_cbk = other_->outstanding_write_callback_;
        other_->outstanding_write_callback_ = {};
        other_->outstanding_write_buffer_ = {};

        read_cbk({}, to_copy);
        write_cbk({}, to_copy);
      });
  } else {
    // Our other side has no write outstanding.

    // If we already have a read outstanding, that means someone
    // called read a second time without waiting for it to complete
    // the first time.
    MJ_ASSERT(outstanding_read_buffer_.size() == 0);

    // If this is a zero byte read, fulfill it immediately.
    if (buffer.size() == 0) {
      pending_read_callback_ = callback;
      parent_->poster_([this]() {
          auto cbk = pending_read_callback_;
          pending_read_callback_ = {};
          cbk({}, 0);
        });
    } else {
      outstanding_read_buffer_ = buffer;
      outstanding_read_callback_ = callback;
    }
  }
}

void StreamPipe::Side::AsyncWriteSome(
    const std::string_view& buffer,
    const SizeCallback& callback) {
  // Does our other side have an outstanding read?
  if (other_->outstanding_read_buffer_.size()) {
    auto to_copy = std::min<std::ptrdiff_t>(
        buffer.size(), other_->outstanding_read_buffer_.size());
    std::memcpy(other_->outstanding_read_buffer_.data(), buffer.data(),
                to_copy);
    pending_write_callback_ = callback;
    parent_->poster_([this, to_copy]() {
        auto write_cbk = this->pending_write_callback_;
        this->pending_write_callback_ = {};

        auto read_cbk = other_->outstanding_read_callback_;
        other_->outstanding_read_callback_ = {};
        other_->outstanding_read_buffer_ = {};

        write_cbk({}, to_copy);
        read_cbk({}, to_copy);
      });
  } else {
    // Our other side has no read outstanding.

    // If we already have a write outstanding, that means someone
    // called read a second time without waiting for it to complete
    // the first time.
    MJ_ASSERT(outstanding_write_buffer_.size() == 0);

    // If this is a zero byte write, fulfill it immediately.
    if (buffer.size() == 0) {
      pending_write_callback_ = callback;
      parent_->poster_([this]() {
          auto cbk = pending_write_callback_;
          pending_write_callback_ = {};
          cbk({}, 0);
        });
    } else {
      outstanding_write_buffer_ = buffer;
      outstanding_write_callback_ = callback;
    }
  }
}

StreamPipe::StreamPipe(EventPoster poster) noexcept
    : poster_(poster),
      side_a_(this, &side_b_),
      side_b_(this, &side_a_) {}

AsyncStream* StreamPipe::side_a() noexcept { return &side_a_; }
AsyncStream* StreamPipe::side_b() noexcept { return &side_b_; }

}
}
