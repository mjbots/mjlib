// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mj_assert.h"
#include "string_view.h"

/// Associates a textual name with a value, up to a fixed maximum
/// number of elements.
template <typename T, std::size_t Size>
class NamedRegistry {
 public:
  struct Element {
    string_view name;
    T value{};
  };

  enum CreateMode {
    kAllowCreate,
    kFindOnly,
  };

  T* FindOrCreate(const string_view& name, CreateMode create_mode) {
    for (auto& element: elements_) {
      if (element.name.size() == 0) {
        switch (create_mode) {
          case kAllowCreate: {
            element.name = name;
            return &element.value;
          }
          case kFindOnly: {
            return nullptr;
          }
        }
      } else if (element.name == name) {
        return &element.value;
      }
    }

    switch (create_mode) {
      case kFindOnly: { break; }
      case kAllowCreate: {
        // Whoops, we ran out of space.
        MJ_ASSERT(false);
      }
    }
    return nullptr;
  }

  T& operator[](std::size_t index) {
    MJ_ASSERT(index < Size);
    return elements_[index].value;
  }

  const T& operator[](std::size_t index) const {
    MJ_ASSERT(index < Size);
    return elements_[index].value;
  }

  std::size_t size() const { return Size; }

  std::array<Element, Size> elements_;
};
