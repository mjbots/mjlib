// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include <cstddef>
#include <cstring>

/// A very simple 'char' only implementation of string_view.
class string_view {
 public:
  using iterator = const char*;
  using const_iterator = const char*;
  using index_type = std::ptrdiff_t;
  using value_type = char;
  using pointer = char*;
  using const_pointer = const char*;
  using reference = char&;
  using const_reference = const char&;

  constexpr string_view() noexcept = default;
  constexpr string_view(const string_view& other) noexcept = default;
  constexpr string_view& operator=(const string_view& other) noexcept = default;
  constexpr string_view(const_pointer ptr, index_type size) : ptr_(ptr), size_(size) {}
  constexpr string_view(iterator begin, iterator end) : ptr_(begin), size_(end - begin) {}

  constexpr const_reference operator[](index_type index) const { return ptr_[index]; }
  constexpr const_reference operator()(index_type index) const { return ptr_[index]; }

  constexpr const_pointer data() const { return ptr_; }

  constexpr index_type length() const noexcept { return size_; }
  constexpr index_type size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr iterator begin() const noexcept { return ptr_; }
  constexpr iterator end() const noexcept { return ptr_ + size_; }

  constexpr const_iterator cbegin() const noexcept { return ptr_; }
  constexpr const_iterator cend() const noexcept { return ptr_ + size_; }

  static string_view ensure_z(const char* str) {
    return string_view(str, std::strlen(str));
  }

 private:
  const char* ptr_ = nullptr;
  index_type size_ = 0;
};
