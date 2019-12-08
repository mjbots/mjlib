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

#include <string_view>

#include "mjlib/base/inplace_function.h"

#include "mjlib/micro/async_exclusive.h"
#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/async_types.h"
#include "mjlib/micro/pool_ptr.h"

namespace mjlib {
namespace micro {

/// This class presents a cmdline interface over an AsyncStream,
/// allowing multiple modules to register commands.
class CommandManager {
 public:
  struct Options {
    int max_line_length = 100;

    Options() {}
  };

  /// @param queue is used to enqueue callbacks
  /// @param read_stream commands are read from this stream
  /// @param write_stream responses are written to this stream
  CommandManager(Pool* pool,
                 AsyncReadStream* read_stream,
                 AsyncExclusive<AsyncWriteStream>* write_stream,
                 const Options& = Options());
  ~CommandManager();

  struct Response {
    AsyncWriteStream* stream = nullptr;
    ErrorCallback callback;

    Response(AsyncWriteStream* stream, ErrorCallback callback)
        : stream(stream), callback(callback) {}
    Response() {}
  };

  using CommandFunction = base::inplace_function<
    void (const std::string_view&, const Response&)>;
  void Register(const std::string_view& name, CommandFunction);

  void AsyncStart();

 private:
  class Impl;
  PoolPtr<Impl> impl_;
};

}
}
