// Copyright 2014-2019 Josh Pieper, jjp@pobox.com.
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
/// In either case, the Serialize or
/// mjlib::base::ExternalSerializer::Serialize method must call the
/// "Visit" method on the archive for each member.  Each call should
/// pass something modeling the NameValuePair concept.

#include <tuple>
#include <type_traits>

#include "mjlib/base/detail/serialize.h"

namespace mjlib {
namespace base {

// template <typename T>
// struct ExternalSerializer {
//   template <typename Archive>
//   void Serialize(T* object, Archive* archive);
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
inline constexpr bool IsSerializable(T* = 0) {
  return mjlib::base::detail::IsSerializable<T>();
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

template <typename RawEnumeration, typename NameMapGetter>
class EnumerationNameValuePair {
 public:
  typedef RawEnumeration Base;
  EnumerationNameValuePair(RawEnumeration* value,
                           const char* name,
                           NameMapGetter mapper)
      : enumeration_mapper(mapper),
        value_(value),
        name_(name) {}

  uint32_t get_value() const { return static_cast<int>(*value_); }
  void set_value(uint32_t value) const {
    *value_ = static_cast<RawEnumeration>(value);
  }
  RawEnumeration* value() const { return value_; }
  const char* name() const { return name_; }

  const NameMapGetter enumeration_mapper;

 private:
  RawEnumeration* const value_;
  const char* const name_;
};

template <typename RawEnumeration, typename NameMapGetter>
EnumerationNameValuePair<RawEnumeration, NameMapGetter>
MakeEnumerationNameValuePair(RawEnumeration* raw_enumeration,
                             const char* name,
                             NameMapGetter getter) {
  return EnumerationNameValuePair<RawEnumeration, NameMapGetter>(
      raw_enumeration, name, getter);
}
}
}

#define MJ_NVP(x) mjlib::base::MakeNameValuePair(&x, #x)
#define MJ_ENUM(x, getter) mjlib::base::MakeEnumerationNameValuePair(&x, #x, getter)
