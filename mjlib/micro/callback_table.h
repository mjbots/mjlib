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

#include "mjlib/base/inplace_function.h"

namespace mjlib {
namespace micro {

/// A helper class to bind arbitrary callbacks into a raw function
/// pointer.  This can be useful for applications where no context can
/// be passed, like an IRQ callback.
///
/// All instances of CallbackTable share the same global storage.
class CallbackTable {
 public:
  using RawFunction = void (*)();

  /// A RAII class that deregisters the callback when destroyed.  A
  /// default constructed instance is valid and has a null
  /// raw_function.
  class Callback {
   public:
    Callback(RawFunction in = nullptr) : raw_function(in) {}
    ~Callback();

    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;

    Callback(Callback&& rhs) : raw_function(rhs.raw_function) {
      rhs.raw_function = nullptr;
    }

    Callback& operator=(Callback&& rhs) {
      raw_function = rhs.raw_function;
      rhs.raw_function = nullptr;
      return *this;
    }

    RawFunction raw_function = nullptr;
  };

  /// Given an arbitrary callback, return a function pointer suitable
  /// for use as an interrupt handler.  When invoked, the given
  /// callback will be called.
  static Callback MakeFunction(mjlib::base::inplace_function<void()> callback);
};

}
}
