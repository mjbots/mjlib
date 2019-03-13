// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include <boost/asio/io_service.hpp>
#include <boost/noncopyable.hpp>

#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

class AsyncReadStream : boost::noncopyable {
 public:
  virtual ~AsyncReadStream() {}

  virtual void async_read_some(MutableBufferSequence, ReadHandler) = 0;
};

class AsyncWriteStream : boost::noncopyable {
 public:
  virtual ~AsyncWriteStream() {}

  virtual void async_write_some(ConstBufferSequence, WriteHandler) = 0;
};

class AsyncStream : public AsyncReadStream, public AsyncWriteStream {
 public:
  ~AsyncStream() override {}

  virtual boost::asio::io_service& get_io_service() = 0;
  virtual void cancel() = 0;
};

using SharedStream = std::shared_ptr<AsyncStream>;
using StreamHandler = std::function<void (const base::error_code&, SharedStream)>;

}
}
