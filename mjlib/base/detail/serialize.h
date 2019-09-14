// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include <type_traits>

#include "mjlib/base/priority_tag.h"

namespace mjlib {
namespace base {

template <typename T>
struct ExternalSerializer {
  using NotSpecialized = int;

  template <typename PairReceiver>
  void Serialize(T* object, PairReceiver);
};

namespace detail {

template <typename T>
constexpr bool IsExternalSerializableImpl(
    PriorityTag<1>,
    typename mjlib::base::ExternalSerializer<T>::NotSpecialized = 0) {
  return false;
}

template <typename T>
constexpr bool IsExternalSerializableImpl(PriorityTag<0>, int = 0) {
  return true;
}

template <typename T>
constexpr bool IsExternalSerializable() {
  return IsExternalSerializableImpl<T>(PriorityTag<1>{});
}

struct NeverUsedTag {};

template <typename T>
constexpr auto IsNativeSerializableImpl(PriorityTag<1>) ->
    std::enable_if_t<
      !std::is_same<
        NeverUsedTag,
        decltype(std::declval<T>().Serialize(
                     static_cast<NeverUsedTag*>(nullptr)))>::value,
      bool> {
  return true;
}

template <typename T>
constexpr bool IsNativeSerializableImpl(PriorityTag<0>) {
  return false;
}

template <typename T>
constexpr bool IsNativeSerializable() {
  return IsNativeSerializableImpl<T>(PriorityTag<1>{});
}

/// This class wraps something that looks like a NameValuePair, but
/// provides an alternate name.
template <typename Base>
class NameValuePairNameOverride : public Base {
 public:
  NameValuePairNameOverride(const Base& base, const char* name_override)
      : Base(base),
        name_override_(name_override) {}

  const char* name() const { return name_override_; }

 private:
  const char* const name_override_;
};


template <typename Serializable, typename Archive>
void SerializeImpl(
    PriorityTag<1>,
    Serializable* serializable,
    Archive* archive,
    std::enable_if_t<IsExternalSerializable<Serializable>(), int> = 0) {
  mjlib::base::ExternalSerializer<Serializable> serializer;
  serializer.Serialize(serializable, [&](auto&& nvp) {
      nvp.value()->Serialize(archive);
    });
}

template <typename Serializable, typename Archive>
void SerializeImpl(
    PriorityTag<0>,
    Serializable* serializable,
    Archive* archive,
    std::enable_if_t<IsNativeSerializable<Serializable>(), int> = 0) {
  serializable->Serialize(archive);
}

template <typename Serializable, typename Archive>
void Serialize(Serializable* serializable, Archive* archive) {
  SerializeImpl(PriorityTag<1>{}, serializable, archive);
}

}
}
}
