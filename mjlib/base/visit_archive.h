// Copyright 2015-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/detail/serialize.h"
#include "mjlib/base/priority_tag.h"
#include "mjlib/base/visitor.h"

namespace mjlib {
namespace base {

/// A helper base class for classes which want to model the Archive
/// concept and differentiate between visited elements which are
/// themselves serializable or not.
template <typename Derived>
struct VisitArchive {
  template <typename Serializable>
  Derived& Accept(Serializable* serializable) {
    mjlib::base::Serialize(serializable, static_cast<Derived*>(this));
    return *static_cast<Derived*>(this);
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& pair) {
    VisitHelper(pair, pair.value(), PriorityTag<1>());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& pair, NameMapGetter) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }

 private:
  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, PriorityTag<1>,
                   std::enable_if_t<IsNativeSerializable<T>(), int> = 0) {
    static_cast<Derived*>(this)->VisitSerializable(pair);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T* value, PriorityTag<1>,
                   std::enable_if_t<IsExternalSerializable<T>(), int> = 0) {
    mjlib::base::ExternalSerializer<T> serializer;
    serializer.Serialize(value, [&](const auto& nvp) {
        detail::NameValuePairNameOverride old_name(nvp, pair.name());
        Visit(old_name);
      });
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, PriorityTag<1>,
                   std::enable_if_t<IsEnum<T>::value, int> = 0) {
    static_cast<Derived*>(this)->VisitEnumeration(pair, &IsEnum<T>::map);
  }

  template <typename NameValuePair, typename T, size_t N>
  int VisitHelper(const NameValuePair& pair, std::array<T, N>*, PriorityTag<1>)  {
    static_cast<Derived*>(this)->VisitArray(pair);
    return 0;
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, PriorityTag<0>) {
    static_cast<Derived*>(this)->VisitScalar(pair);
  }
};

}
}
