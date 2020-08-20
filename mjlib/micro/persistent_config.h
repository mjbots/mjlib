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

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/command_manager.h"
#include "mjlib/micro/flash.h"
#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/serializable_handler.h"

namespace mjlib {
namespace micro {

class PersistentConfig {
 public:
  PersistentConfig(Pool&, CommandManager&, FlashInterface&);
  ~PersistentConfig();

  struct RegisterOptions {
    bool enumerate = true;

    RegisterOptions() {}
  };

  /// Associate the given serializable with the given name.
  ///
  /// Both @p name and @p serializable are aliased internally and must
  /// remain valid forever.
  template <typename Serializable>
  void Register(const std::string_view& name, Serializable* serializable,
                base::inplace_function<void ()> updated,
                const RegisterOptions& options = RegisterOptions()) {
    PoolPtr<SerializableHandler<Serializable>> concrete(pool(), serializable);
    RegisterDetail(name, concrete.get(), updated, options);
  }

  /// Restore all registered configuration structures from Flash.
  /// This should be invoked after all modules have had a chance to
  /// register their configurables.
  void Load();

 private:
  /// This aliases Base, which must remain valid for the lifetime of
  /// the PersistentConfig.
  void RegisterDetail(const std::string_view& name, SerializableHandlerBase*,
                      base::inplace_function<void ()> updated,
                      const RegisterOptions&);

  Pool* pool() const;

  class Impl;
  PoolPtr<Impl> impl_;
};

}
}
