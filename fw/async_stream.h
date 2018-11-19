// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "NonCopyable.h"

#include "async_types.h"
#include "static_function.h"
#include "string_span.h"
#include "string_view.h"

class AsyncReadStream : mbed::NonCopyable<AsyncReadStream> {
 public:
  virtual ~AsyncReadStream() {}

  virtual void AsyncReadSome(const string_span&, const SizeCallback&) = 0;
};

class AsyncWriteStream : mbed::NonCopyable<AsyncWriteStream> {
 public:
  virtual ~AsyncWriteStream() {}

  virtual void AsyncWriteSome(const ::string_view&, const SizeCallback&) = 0;
};

class AsyncStream : public AsyncReadStream, public AsyncWriteStream {
 public:
  ~AsyncStream() override {}
};

template <typename Stream>
void AsyncWrite(Stream& stream, const ::string_view& data, const ErrorCallback& callback) {
  if (data.empty()) {
    callback(0);
    return;
  }

  auto continuation = [stream=&stream, data, cbk=callback.shrink<4>()]
      (ErrorCode error, ssize_t size) {
    if (error) {
      cbk(error);
      return;
    }

    if (data.size() == size) {
      cbk(0);
      return;
    }

    AsyncWrite(*stream, ::string_view(data.begin() + size, data.end()), cbk);
  };

  stream.AsyncWriteSome(data, continuation);
}

template <typename Stream>
void AsyncRead(Stream& stream, const string_span& data, const ErrorCallback& callback) {
  if (data.empty()) {
    callback(0);
    return;
  }

  auto continuation = [stream=&stream, data, cbk=callback.shrink<4>()]
      (ErrorCode error, ssize_t size) {
    if (error) {
      cbk(error);
      return;
    }
    if (data.size() == size) {
      cbk(0);
      return;
    }

    AsyncRead(*stream, string_span(data.begin() + size, data.end()), cbk);
  };

  stream.AsyncReadSome(data, continuation);
}
