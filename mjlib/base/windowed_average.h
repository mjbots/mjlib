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

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "mjlib/base/assert.h"

namespace mjlib {
namespace base {

template <typename T, size_t MaxCapacity,  typename AccumType>
class WindowedAverage {
 public:
  WindowedAverage(size_t capacity = MaxCapacity)
      : capacity_(std::min<size_t>(MaxCapacity, capacity)) {
    // capacity_ == 0 would cause Add() to compute (pos_ + 1) % 0,
    // which is UB and on x86_64 raises SIGFPE.
    MJ_ASSERT(capacity > 0);
  }

  static_assert(MaxCapacity > 0, "MaxCapacity must be at least 1");
  static_assert(
      std::is_integral_v<T>,
      "this will not function with floating point rounding error");
  static_assert(std::is_signed_v<T>, "this requires a signed type");
  static_assert(std::numeric_limits<T>::digits <
                std::numeric_limits<int64_t>::digits);

  void Add(T value) {
    const auto old = data_[pos_];
    total_ += value;
    data_[pos_] = value;
    pos_ = (pos_ + 1) % capacity_;
    const auto old_size = size_;
    size_ = std::min(size_ + 1, capacity_);
    if (old_size == size_) {
      total_ -= old;
    }
  }

  T average() const {
    if (size_ == 0) { return T(); }
    // Divide in the accumulator type so size_ is not narrowed to T
    // (e.g. T = int16_t with a window of 40000 wrapped the divisor
    // to a negative value, producing wrong-signed results). The
    // quotient fits in T whenever the inputs to Add() fit in T.
    return total_ / static_cast<AccumType>(size_);
  }

  AccumType total() const { return total_; }
  size_t size() const { return size_; }

 private:
  T data_[MaxCapacity] = {};
  AccumType total_ = 0;
  size_t capacity_ = 0;
  size_t size_ = 0;
  size_t pos_ = 0;
};

}
}
