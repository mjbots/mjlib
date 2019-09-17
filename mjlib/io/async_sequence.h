// Copyright 2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include <memory>
#include <string_view>

#include <boost/asio/io_service.hpp>

#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

/// Execute a list of chainable callbacks one after another.  If any
/// returns an error, abort immediately to the final @p
/// completion_callback.
///
/// There wouldn't be much need for this if we had coroutines.
class AsyncSequence {
 public:
  AsyncSequence(boost::asio::io_context&);

  /// Append a new operation to the list.
  AsyncSequence& op(ChainableCallback, std::string_view description = "");

  /// This must the be last method called, it initiates the sequence.
  void Start(ErrorCallback);

 private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

}
}
