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

#include "async_types.h"

namespace mjlib {
namespace micro {

/// Create a callback which must be invoked exactly once before this
/// class is destroyed.
class RequiredSuccess {
 public:
  ErrorCallback Make() {
    return [this](error_code ec) {
      MJ_ASSERT(!done_);
      MJ_ASSERT(!ec);
      done_ = true;
    };
  }

  ~RequiredSuccess() {
    MJ_ASSERT(done_);
  }

 private:
  bool done_ = false;
};

}
}
