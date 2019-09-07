// Copyright 2015 Josh Pieper, jjp@pobox.com.
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

#include "pool_ptr.h"

#include <memory>

#include "mjlib/base/assert.h"

namespace mjlib {
namespace micro {

void* Pool::Allocate(std::size_t size, std::size_t alignment) {
  void* ptr = data_ + position_;
  std::size_t space = size_ - position_;
  const auto result = std::align(alignment, size, ptr, space);

  MJ_ASSERT(result != nullptr);

  position_ = reinterpret_cast<char*>(result) - data_ + size;
  return result;
}

}
}
