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

#include <array>
#include <optional>

#include "mjlib/base/visit_archive.h"

namespace mjlib {
namespace telemetry {
namespace detail {
template <typename T>
struct FakeNvp {
  FakeNvp(T* value) : value_(value) {}
  const T& get_value() const { return *value_; }
  T* value() const { return value_; }
  void set_value(T value) const { *value_ = value; }

  T* const value_;
};

template <typename T>
FakeNvp<T> MakeFakeNvp(T* value) { return FakeNvp<T>(value); }

template <typename Derived, typename Stream>
class DataVisitorBase : public base::VisitArchive<Derived> {
 public:
  DataVisitorBase(Stream& stream) : stream_(stream) {}

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& pair) {
    Derived sub(stream_);
    sub.Accept(pair.value());
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    typename std::decay<decltype(pair.get_value())>::type * dummy = nullptr;
    VisitHelper(pair, dummy, 0);
  }

  template <typename NameValuePair, typename First, typename Second>
  void VisitHelper(const NameValuePair& pair,
                   std::pair<First, Second>*,
                   int) {
    base::VisitArchive<Derived>::Visit(
        FakeNvp<First>(&pair.value()->first));
    base::VisitArchive<Derived>::Visit(
        FakeNvp<Second>(&pair.value()->second));
    }

  template <typename NameValuePair, typename T, std::size_t N>
  void VisitHelper(const NameValuePair& pair,
                   std::array<T, N>*,
                   int) {
    auto value = pair.value();
    for (std::size_t i = 0; i < N; i++) {
      base::VisitArchive<Derived>::Visit(FakeNvp<T>(&(*value)[i]));
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   std::vector<T>*,
                   int) {
    static_cast<Derived*>(this)->VisitVector(pair);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   std::optional<T>*,
                   int) {
    static_cast<Derived*>(this)->VisitOptional(pair);
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& pair,
                   std::string*,
                   int) {
    static_cast<Derived*>(this)->VisitString(pair);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   T*,
                   long) {
    static_cast<Derived*>(this)->VisitPrimitive(pair);
  }

 protected:
  Stream& stream_;
};

}
}
}
