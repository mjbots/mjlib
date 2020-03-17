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

#include <cstdlib>
#include <memory>

namespace mjlib {
namespace micro {

/// Holds a single instance of an object while consuming a fixed
/// compile time specified size.  A compiler error is emitted if the
/// object requires more space than available.  This can be used to
/// implement impl patterns where dynamic allocation is not allowed
/// for instance.
template <typename T, std::size_t Capacity>
class StaticPtr {
 public:

  StaticPtr() : present_(false) {}

  template <typename... Args>
  StaticPtr(Args&&... args)
      : present_(true) {
    new (get()) T(std::forward<Args>(args)...);
  }

  StaticPtr(StaticPtr&& rhs)
      : present_(!!rhs) {
    new (get()) T(std::move(*rhs.get()));
    rhs.reset();
  }

  void swap(StaticPtr& rhs) {
    if (present_ && !!rhs) {
      std::swap(*rhs, *this);
      std::swap(present_, rhs.present_);
    } else if (present_ != !!rhs) {
      if (present_) {
        rhs = std::move(*this);
      } else {
        *this = std::move(rhs);
      }
    } else {
      // We are both empty already.
    }
  }

  StaticPtr& operator=(StaticPtr&& rhs) {
    reset();
    present_ = !!rhs;
    if (present_) {
      new (get()) T(std::move(*rhs.get()));
    }
    rhs.reset();
    return *this;
  }

  ~StaticPtr() {
    // We put this here, so that it is only evaluated when the full
    // type is known.
    static_assert(sizeof(T) <= Capacity);
    reset();
  }

  void reset() {
    if (present_) {
      get()->~T();
    }
    present_ = false;
  }

  T& operator*() { return *get(); }
  const T& operator*() const { return *get(); }

  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

  operator bool() const { return present_; }

 private:
  T* get() {
    if (!present_) { return nullptr; }
    void* ptr = data_;
    std::size_t space = sizeof(data_);
    return reinterpret_cast<T*>(
        std::align(alignof(T), sizeof(T), ptr, space));
  }

  const T* get() const {
    if (!present_) { return nullptr; }
    void* ptr = const_cast<char*>(&data_[0]);
    std::size_t space = sizeof(data_);
    return reinterpret_cast<const T*>(
        std::align(alignof(T), sizeof(T), ptr, space));
  }

  bool present_ = false;
  char data_[Capacity] = {};
};

}
}
