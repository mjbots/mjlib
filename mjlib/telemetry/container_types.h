// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {
namespace detail {

template <typename T>
bool IsContainerTypeMatch(std::vector<T>*, Format::Type type) {
  return type == Format::Type::kArray;
}

template <typename T, size_t N>
bool IsContainerTypeMatch(std::array<T, N>*, Format::Type type) {
  return type == Format::Type::kFixedArray;
}

template <typename T>
bool IsContainerTypeMatch(std::optional<T>*, Format::Type type) {
  return type == Format::Type::kUnion;
}

template <typename T>
bool IsContainerTypeMatch(std::map<std::string, T>*, Format::Type type) {
  return type == Format::Type::kMap;
}

template <typename T>
bool IsContainerTypeMatch(T*, Format::Type type,
                          std::enable_if_t<base::IsEnum<T>::value, int> = 0) {
  return type == Format::Type::kEnum;
}

template <typename T>
bool IsContainerTypeMatch(T*, Format::Type,
                          std::enable_if_t<!base::IsEnum<T>::value, int> = 0) {
  return false;
}

}  // namespace detail
}  // namespace telemetry
}  // namespace mjlib
