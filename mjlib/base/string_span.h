// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include <cstddef>
#include <cstring>
#include <string>

namespace mjlib {
namespace base {

/// A very simple 'char' only implementation of string_span.
class string_span {
 public:
  using iterator = char*;
  using const_iterator = const char*;
  using index_type = std::ptrdiff_t;
  using value_type = char;
  using pointer = char*;
  using const_pointer = const char*;
  using reference = char&;
  using const_reference = const char&;

  constexpr string_span() noexcept = default;
  constexpr string_span(const string_span& other) noexcept = default;
  constexpr string_span& operator=(const string_span& other) noexcept = default;
  constexpr string_span(pointer ptr, index_type size) : ptr_(ptr), size_(size) {}
  constexpr string_span(iterator begin, iterator end) : ptr_(begin), size_(end - begin) {}
  string_span(std::string& data) : ptr_(&data[0]), size_(static_cast<index_type>(data.size())) {}

  template <std::size_t Size>
  constexpr string_span(char (&array)[Size]) : ptr_(array), size_(Size) {}

  constexpr reference operator[](index_type index) const { return ptr_[index]; }
  constexpr reference operator()(index_type index) const { return ptr_[index]; }

  constexpr pointer data() const { return ptr_; }

  constexpr index_type length() const noexcept { return size_; }
  constexpr index_type size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr iterator begin() const noexcept { return ptr_; }
  constexpr iterator end() const noexcept { return ptr_ + size_; }

  constexpr const_iterator cbegin() const noexcept { return ptr_; }
  constexpr const_iterator cend() const noexcept { return ptr_ + size_; }

  static string_span ensure_z(char* str) {
    return string_span(str, std::strlen(str));
  }

 private:
  char* ptr_ = nullptr;
  index_type size_ = 0;
};

}
}
