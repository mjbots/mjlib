// Copyright 2014-2020 Josh Pieper, jjp@pobox.com.
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

///@file
///
/// These classes and macros support instances of the Visitor pattern.
///
/// Objects can configure to be visited by either:
///
///  1) implementing a templated "Serialize" method, which takes a single
///     templated "Archive*" argument.
///   OR
///  2) specializing the mjlib::base::ExternalSerializer class
///     template for the given type
///
/// With method 1, the Serialize method must call the "Visit" method
/// on the archive for each member.  Each call should pass something
/// modeling the NameValuePair concept.
///
/// With method 2, the external serializer needs to call the passed
/// function object with something modeling the NameValuePair concept
/// which satisfies method 1.

#include <tuple>
#include <type_traits>

#include "mjlib/base/detail/serialize.h"

namespace mjlib {
namespace base {

// template <typename T>
// struct ExternalSerializer {
//   template <typename PairReceiver>
//   void Serialize(T* object, PairReceiver);
// };

/// This function object may be used to invoke the "correct" Serialize
/// method for a given type.
inline constexpr auto Serialize =
    [](auto&& object, auto&& archive) ->
    decltype(mjlib::base::detail::Serialize(std::forward<decltype(object)>(object), std::forward<decltype(archive)>(archive))) {
  return mjlib::base::detail::Serialize(std::forward<decltype(object)>(object), std::forward<decltype(archive)>(archive));
};

/// This returns 'true' if the object is a structure which can be
/// serialized.
template <typename T>
inline constexpr bool IsNativeSerializable(T* = 0) {
  return mjlib::base::detail::IsNativeSerializable<T>();
}

/// This returns 'true' if the object is a structure which has an
/// external serializer defined.
template <typename T>
inline constexpr bool IsExternalSerializable(T* = 0) {
  return mjlib::base::detail::IsExternalSerializable<T>();
}

template <typename T>
inline constexpr bool IsSerializable(T* = 0) {
  return IsNativeSerializable<T>() || IsExternalSerializable<T>();
}

/// template <typename T>
/// class NameValuePair {
///  public:
///   const char* name() const;
///   T get_value() const; // may return a T or const T&
///   void set_value(T); // may take a T or const T&
/// };


/// A class which models the NameValuePair concept that stores a
/// pointer to the underlying data.
template <typename T>
class ReferenceNameValuePair {
 public:
  ReferenceNameValuePair(T* value, const char* name)
      : value_(value), name_(name) {}
  const T& get_value() const { return *value_; }
  void set_value(const T& value) const { *value_ = value; }

  T* value() const { return value_; }

  const char* name() const { return name_; }

 private:
  T* const value_;
  const char* const name_;
};

template <typename T>
ReferenceNameValuePair<T> MakeNameValuePair(T* value, const char* name) {
  return ReferenceNameValuePair<T>(value, name);
}

/// Clients can specialize this structure to declare enumeration
/// types.
template <typename T>
struct IsEnum {
  static constexpr bool value = false;

  /// Any specialization should declare the following.
  // using NameMapGetter = X;
};

}
}

#define MJ_NVP(x) ::mjlib::base::MakeNameValuePair(&x, #x)
