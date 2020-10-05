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

#include <cstddef>
#include <cstdlib>

#include <boost/iterator/iterator_facade.hpp>

#include "mjlib/base/copy_const.h"

namespace mjlib {
namespace micro {

/// A dynamic vector with a fixed maximum size.
template <typename T, size_t Capacity>
class StaticVector {
 public:
  template <typename Value>
  class iterator_base : public boost::iterator_facade<
   iterator_base<Value>, Value, boost::random_access_traversal_tag> {
   public:
    iterator_base() : parent_(nullptr), index_(0) {}
    iterator_base(typename base::CopyConst<
                  Value, StaticVector>::type* parent, std::ptrdiff_t index)
        : parent_(parent), index_(index) {}
    template <class OtherValue>
    iterator_base(const iterator_base<OtherValue>& other)
        : parent_(other.parent), index_(other.index) {}

   private:
    friend class boost::iterator_core_access;
    template <class> friend class iterator_base;

    void increment() { index_++; }
    void decrement() { index_--; }

    template <typename OtherValue>
    bool equal(const OtherValue& other) const {
      return parent_ == other.parent_ && index_ == other.index_;
    }

    Value& dereference() const { return parent_->data_[index_]; }

    void advance(std::ptrdiff_t offset) {
      index_ += offset;
    }

    template <typename OtherValue>
    std::ptrdiff_t distance_to(const OtherValue& other) const {
      return other.index_ - index_;
    }

    typename base::CopyConst<Value, StaticVector>::type* parent_;
    std::ptrdiff_t index_;
  };

  using iterator = iterator_base<T>;
  using const_iterator = iterator_base<const T>;

  StaticVector() {}
  StaticVector(std::ptrdiff_t count,
               const T& value_in = T()) {
    size_ = count;
    for (auto& value : *this) { value = value_in; }
  }

  template <class InputIterator>
  StaticVector(InputIterator first, InputIterator last) {
    for (auto it = first; it != last; ++it) {
      data_[size_] = *it;
      size_++;
    }
  }

  StaticVector(std::initializer_list<T> init) {
    for (auto value : init) {
      data_[size_] = value;
      size_++;
    }
  }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size_); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size_); }

  void push_back(T value) {
    data_[size_] = std::move(value);
    size_++;
  }

  void pop_back() {
    size_--;
  }

  bool empty() const {
    return size_ == 0;
  }

  std::ptrdiff_t size() const {
    return size_;
  }

  std::ptrdiff_t capacity() const {
    return Capacity;
  }

  void clear() {
    size_ = 0;
  }

  T& front() { return data_[0]; }
  const T& front() const { return data_[0]; }
  T& back() { return data_[size_ - 1]; }
  const T& back() const { return data_[size_ - 1]; }
  T* data() { return &data_[0]; }
  const T* data() const { return &data_[0]; }

  T& operator[](std::ptrdiff_t index) { return data_[index]; }
  const T& operator[](std::ptrdiff_t index) const { return data_[index]; }

 private:
  std::ptrdiff_t size_ = 0;
  T data_[Capacity] = {};
};

}
}
