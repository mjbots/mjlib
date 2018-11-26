// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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
#include <cstdlib>

namespace mjlib {
namespace base {

template <typename T, size_t Size>
class WindowedAverage {
 public:
  void Add(T value) {
    data_[pos_] = value;
    pos_ = (pos_ + 1) % Size;
    size_ = std::min(size_ + 1, Size);
  }

  T average() const {
    if (size_ == 0) { return T(); }
    T total{};
    size_t place = pos_;
    for (size_t i = 0; i < size_; i++) {
      place = (place + Size - 1) % Size;
      total += data_[place];
    }
    return total / static_cast<T>(size_);
  }

 private:
  T data_[Size] = {};
  size_t size_ = 0;
  size_t pos_ = 0;
};

}
}
