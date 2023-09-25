// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/micro/pool_ptr.h"

namespace mjlib {
namespace micro {

// A container similar to std::array, but allocates a runtime
// determined number of elements from a Pool.
template <typename Value>
class PoolArray {
 public:
  using value_type = Value;
  using iterator = Value*;
  using const_iterator = const Value*;

  PoolArray(Pool* pool, size_t size) : size_(size) {
    data_ = static_cast<Value*>(pool->Allocate(sizeof(Value) * size_,
                                               alignof(Value)));
    for (size_t i = 0; i < size_; i++) {
      ::new (data_ + i) Value{};
    }
  }

  ~PoolArray() {
    for (size_t i = 0; i < size_; i++) {
      (&data_[i])->~Value();
    }
  }

  iterator begin() { return data_; }
  iterator end() { return data_ + size_; }

  const_iterator begin() const { return data_ ; }
  const_iterator end() const { return data_ + size_; }

  Value& operator[](size_t i) {
    return data_[i];
  }

  const Value& operator[](size_t i) const {
    return data_[i];
  }

 private:
  Value* data_;
  const size_t size_;
};

}
}
