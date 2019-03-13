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

#include <string_view>

#include "mjlib/base/noncopyable.h"
#include "mjlib/base/string_span.h"

#include "mjlib/micro/async_types.h"
#include "mjlib/micro/error_code.h"
#include "mjlib/micro/static_function.h"

namespace mjlib {
namespace micro {

class AsyncReadStream : base::NonCopyable {
 public:
  virtual ~AsyncReadStream() {}

  virtual void AsyncReadSome(const base::string_span&, const SizeCallback&) = 0;
};

class AsyncWriteStream : base::NonCopyable {
 public:
  virtual ~AsyncWriteStream() {}

  virtual void AsyncWriteSome(const std::string_view&, const SizeCallback&) = 0;
};

class AsyncStream : public AsyncReadStream, public AsyncWriteStream {
 public:
  ~AsyncStream() override {}
};

template <typename Stream>
void AsyncWrite(Stream& stream, const std::string_view& data,
                const ErrorCallback& callback) {
  if (data.empty()) {
    callback({});
    return;
  }

  auto continuation = [stream=&stream, data, cbk=callback.shrink<4>()]
      (error_code error, ssize_t size) {
    if (error) {
      cbk(error);
      return;
    }

    if (static_cast<ssize_t>(data.size()) == size) {
      cbk({});
      return;
    }

    AsyncWrite(*stream,
               std::string_view(data.begin() + size, data.size() - size),
               cbk);
  };

  stream.AsyncWriteSome(data, continuation);
}

template <typename Stream>
void AsyncRead(Stream& stream, const base::string_span& data,
               const ErrorCallback& callback) {
  if (data.empty()) {
    callback({});
    return;
  }

  auto continuation = [stream=&stream, data, cbk=callback.shrink<4>()]
      (error_code error, ssize_t size) {
    if (error) {
      cbk(error);
      return;
    }
    if (data.size() == size) {
      cbk({});
      return;
    }

    AsyncRead(*stream,
              base::string_span(data.begin() + size, data.end()),
              cbk);
  };

  stream.AsyncReadSome(data, continuation);
}

}
}
