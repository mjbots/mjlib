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

#include "mjlib/base/inplace_function.h"

#include "mjlib/micro/async_exclusive.h"
#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/command_manager.h"
#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/serializable_handler.h"

namespace mjlib {
namespace micro {

/// The telemetry manager enables live introspection into arbitrary
/// serializable structures.
class TelemetryManager {
 public:
  TelemetryManager(Pool*,
                   CommandManager*,
                   AsyncExclusive<AsyncWriteStream>* write_stream);
  ~TelemetryManager();

  /// Associate the serializable with the given name.
  ///
  /// Both @p name and @p serializable are aliased internally and must
  /// remain valid for the life of this instance.
  ///
  /// @return a function which can be used to indicate that a new
  /// version of the structure is available.
  template <typename Serializable>
  base::inplace_function<void ()> Register(
      const std::string_view& name, Serializable* serializable) {
    PoolPtr<SerializableHandler<Serializable>> concrete(pool(), serializable);
    return RegisterDetail(name, concrete.get());
  }

  /// This should be invoked every millisecond.
  void PollMillisecond();

 private:
  base::inplace_function<void ()> RegisterDetail(
      const std::string_view& name, SerializableHandlerBase*);

  Pool* pool() const;

  class Impl;
  PoolPtr<Impl> impl_;
};

}
}
