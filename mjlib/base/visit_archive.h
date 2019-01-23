// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

namespace mjlib {
namespace base {

/// A helper base class for classes which want to model the Archive
/// concept and differentiate between visited elements which are
/// themselves serializable or not.
template <typename Derived>
struct VisitArchive {
  template <typename Serializable>
  Derived& Accept(Serializable* serializable) {
    serializable->Serialize(static_cast<Derived*>(this));
    return *static_cast<Derived*>(this);
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& pair) {
    VisitHelper(pair, pair.value(), 0);
  }

  template <typename NameValuePair>
  void VisitEnumeration(const NameValuePair& pair) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }

 private:
  template <typename NameValuePair, typename T>
  auto VisitHelper(const NameValuePair& pair, T*, int) ->
      decltype(pair.value()->Serialize(
                   static_cast<Derived*>(nullptr))) {
    static_cast<Derived*>(this)->VisitSerializable(pair);
  }

  template <typename NameValuePair, typename T>
  auto VisitHelper(const NameValuePair& pair, T*, int) ->
      decltype(pair.enumeration_mapper) {
    static_cast<Derived*>(this)->VisitEnumeration(pair);
    return pair.enumeration_mapper;
  }

  template <typename NameValuePair, typename T, size_t N>
  int VisitHelper(const NameValuePair& pair, std::array<T, N>*, int)  {
    static_cast<Derived*>(this)->VisitArray(pair);
    return 0;
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, long) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }
};

}
}
