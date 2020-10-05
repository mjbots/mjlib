// Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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

#include <cmath>
#include <type_traits>

namespace mjlib {
namespace base {

namespace detail {

template <typename T>
T LimitIntegral(T a, T min, T max) {
  if (a < min) { return min; }
  if (a > max) { return max; }
  return a;
}

template <typename T>
T LimitFloat(T a, T min, T max) {
  if (!std::isnan(min) && a < min) { return min; }
  if (!std::isnan(max) && a > max) { return max; }
  return a;
}

template <typename T>
T LimitDispatch(T a, T min, T max, const std::true_type&) {
  return LimitFloat(a, min, max);
}

template <typename T>
T LimitDispatch(T a, T min, T max, const std::false_type&) {
  return LimitIntegral(a, min, max);
}
}  // namespace detail

template <typename T>
T Limit(T a, T min, T max) {
  return detail::LimitDispatch(a, min, max, std::is_floating_point<T>());
}

}
}
