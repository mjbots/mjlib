// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include <cstddef>
#include <utility>

/// Holds an instance of T as if by value, however is guaranteed to
/// always be of size Size and the definition of T is not required to
/// be in scope unless it is dereferenced.
///
/// This is mostly useful for Pimpl patterns where heap allocation is
/// not permitted.
template <typename T, size_t Size>
class OpaquePtr {
 public:
  template <typename... Args>
  OpaquePtr(Args&&... args) {
    static_assert(sizeof(T) <= Size, "declared size is insufficient");
    new (storage_) T(std::forward<Args>(args)...);
  }

  ~OpaquePtr() {
    (*this)->~T();
  }

  // Delete the copy and assignment operators.
  OpaquePtr(const OpaquePtr&) = delete;
  OpaquePtr& operator=(const OpaquePtr&) = delete;

  T* operator->() { return &(**this); }
  const T* operator->() const { &(**this); }
  T& operator*() { return *reinterpret_cast<T*>(data()); }
  const T& operator*() const { return *reinterpret_cast<const T*>(data()); }

 private:
  // char[] != char* for the purposes of strict aliasing, so we use
  // these helpers to get us to char*.
  char* data() { return storage_; }
  const char* data() const { return storage_; }

  alignas(std::max_align_t) char storage_[Size] = {};
};
