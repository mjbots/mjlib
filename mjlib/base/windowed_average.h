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

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace mjlib {
namespace base {

template <typename T, size_t Size,  typename AccumType>
class WindowedAverage {
 public:
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
    pos_ = (pos_ + 1) % Size;
    const auto old_size = size_;
    size_ = std::min(size_ + 1, Size);
    if (old_size == size_) {
      total_ -= old;
    }
  }

  T average() const {
    if (size_ == 0) { return T(); }
    return total_ / static_cast<T>(size_);
  }

 private:
  T data_[Size] = {};
  AccumType total_ = 0;
  size_t size_ = 0;
  size_t pos_ = 0;
};

}
}
