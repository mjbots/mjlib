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

#include <functional>

#include "pool_ptr.h"

namespace mjlib {
namespace micro {

/// A container with an interface similar to std::map, but that
/// allocates a fixed amount of memory from a Pool.  It preserves
/// addresses of elements, but has lookup linear in the size of the
/// container.
template <typename Key, typename Value, class Compare = std::less<Key>>
class PoolMap {
 public:
  using Node = std::pair<Key, Value>;

  using key_type = Key;
  using mapped_type = Value;
  using value_type = Node;

  using iterator = Node*;
  using const_iterator = const Node*;

  PoolMap(Pool* pool, size_t max_elements)
      : max_size_(max_elements) {
    data_ = static_cast<Node*>(pool->Allocate(sizeof(Node) * max_elements,
                                              alignof(Node)));
    for (size_t i = 0; i < max_elements; i++) {
      ::new (data_ + i) Node{};
    }
  }

  ~PoolMap() {
    for (size_t i = 0; i < max_size_; i++) {
      (&data_[i])->~Node();
    }
  }

  iterator begin() { return data_; }
  iterator end() { return data_ + size_; }

  const_iterator begin() const { return data_; }
  const_iterator end() const { return data_ + size_; }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  Node& operator[](size_t i) {
    return data_[i];
  }

  const Node& operator[](size_t i) const {
    return data_[i];
  }

  std::pair<iterator, bool> insert(const value_type& value) {
    Node* iter = begin();
    Node* const stop = end();
    Compare comparator;
    for (; iter != stop; ++iter) {
      if (!comparator(iter->first, value.first) &&
          !comparator(value.first, iter->first)) {
        return std::make_pair(iter, false);
      }
    }
    MJ_ASSERT((iter - begin()) < static_cast<std::ptrdiff_t>(max_size_));
    size_++;
    *iter = value;
    return std::make_pair(iter, true);
  }

  iterator find(const Key& key) {
    const auto stop = end();
    Compare comparator;
    for (iterator it = begin(); it != stop; ++it) {
      if (!comparator(it->first, key) &&
          !comparator(key, it->first)) {
        return it;
      }
    }
    return stop;
  }

  const_iterator find(const Key& key) const {
    const auto stop = end();
    Compare comparator;
    for (const_iterator it = begin(); it != stop; ++it) {
      if (!comparator(it->first, key) &&
          !comparator(key, it->first)) {
        return it;
      }
    }
    return stop;
  }

  bool contains(const Key& key) const {
    Compare comparator;
    for (auto& item : *this) {
      if (!comparator(item.first, key) &&
          !comparator(key, item.first)) {
        return true;
      }
    }
    return false;
  }

 private:
  Node* data_;
  size_t size_ = 0;
  const size_t max_size_;
};

}
}
