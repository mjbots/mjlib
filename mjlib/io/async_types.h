// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include <boost/asio/buffer.hpp>

#include <function2/function2.hpp>

#include "mjlib/base/error_code.h"

namespace mjlib {
namespace io {

// Concrete types that model various callbacks, including concepts
// from boost::asio.
using VoidCallback = fu2::unique_function<void ()>;
using ErrorCallback = fu2::unique_function<void (const base::error_code&)>;
using SizeCallback = fu2::unique_function<void (const base::error_code&, size_t)>;
using ChainableCallback = fu2::unique_function<void (ErrorCallback)>;

using ReadHandler = SizeCallback;
using WriteHandler = SizeCallback;

// And concrete types that model the various boost::asio buffer
// types.
template <typename Buffer>
class BufferSequence {
 public:
  typedef Buffer value_type;
  typedef typename std::vector<value_type>::const_iterator const_iterator;

  BufferSequence() {}

  template <typename Sequence>
  BufferSequence(const Sequence& s) : data_(s.begin(), s.end()) {}

  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }

 protected:
  std::vector<value_type> data_;
};

class MutableBufferSequence
    : public BufferSequence<boost::asio::mutable_buffer> {
 public:
  MutableBufferSequence() {}

  template <typename Buffers>
  MutableBufferSequence(const Buffers& t)
  : BufferSequence<boost::asio::mutable_buffer>(t) {}
};

class ConstBufferSequence : public BufferSequence<boost::asio::const_buffer> {
 public:
  ConstBufferSequence() {}

  template <typename Buffers>
  ConstBufferSequence(const Buffers& t)
  : BufferSequence<boost::asio::const_buffer>(t) {}
};

}
}
