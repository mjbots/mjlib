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

#include <clipp/clipp.h>

#include <fmt/format.h>

#include "mjlib/base/system_error.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/visit_archive.h"

#include "mjlib/base/priority_tag.h"

namespace mjlib {
namespace base {

/// Iterate over a serializable and return a clipp::group for every
/// element.
class ClippArchive : public VisitArchive<ClippArchive> {
 public:
  ClippArchive(std::string prefix = "")
      : prefix_(prefix) {}

  template <typename ValueType>
  clipp::group Value(ValueType* value, const char* name) {
    ClippArchive sub;
    base::ReferenceNameValuePair nvp(value, name);
    sub.VisitArchive<ClippArchive>::Visit(nvp);
    return sub.release();
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    VisitHelper(pair, pair.value(), PriorityTag<1>());
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& pair) {
    group_.merge(
        clipp::with_prefix(std::string(pair.name()) + ".", ClippArchive()
                           .Accept(pair.value()).release()));
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    for (size_t i = 0; i < pair.value()->size(); i++) {
      const auto name = fmt::format("{}.{}", pair.name(), i);
      group_.merge(
          ClippArchive::Value(&(*pair.value())[i], name.c_str()));
    }
  }


  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp,
                        NameMapGetter enumeration_mapper) {
    auto option =
        clipp::option(MakeName(nvp)) &
        clipp::value(
            get_label(nvp, PriorityTag<1>()),
            [nvp, enumeration_mapper](std::string value) {
              for (const auto& pair : enumeration_mapper()) {
                if (value == pair.second) {
                  nvp.set_value(pair.first);
                  return;
                }
              }
              system_error::throw_if(
                  true, fmt::format(
                      "invalid enum: {} not in {}",
                      value, allowable_enums(enumeration_mapper)));
            });
    option = SetOptions(option, nvp, PriorityTag<1>());
    option = option.doc(option.doc() + " (" +
                        allowable_enums(enumeration_mapper) + ")");
    group_.push_back(ensure_doc(option));
  }

  clipp::group release() {
    return std::move(group_);
  }

  clipp::group group() {
    return group_;
  }

 private:
  template <typename Mapper>
  static std::string allowable_enums(Mapper mapper) {
    std::string result;
    bool first = true;
    for (auto pair : mapper()) {
      if (!first) {
        result += "/";
      }
      first = false;
      result += pair.second;
    }
    return result;
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::vector<T>*,
                   PriorityTag<1>) {
    // Ignore vectors.
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp, T* storage, PriorityTag<0>) {
    group_.push_back(
        ensure_doc(
            SetOptions(
                (clipp::option(MakeName(nvp)) &
                 clipp::value(get_label(nvp, PriorityTag<1>()), *storage)),
                nvp,
                PriorityTag<1>())));
  }

  template <typename NameValuePair>
  std::string MakeName(const NameValuePair& nvp) const {
    return prefix_ + nvp.name();
  }

  template <typename Option, typename NameValuePair>
  Option SetOptions(Option option, const NameValuePair& nvp, PriorityTag<1>,
                    decltype(&NameValuePair::argument_traits) = 0) {
    auto result = option;
    result =  nvp.argument_traits().help ?
        result.doc(nvp.argument_traits().help) :
        result;
    return result;
  }

  template <typename Option, typename NameValuePair>
  Option SetOptions(Option option, const NameValuePair&, PriorityTag<0>) {
    return option;
  }

  template <typename Option>
  Option ensure_doc(Option option) {
    // Without at least some docstring, options that take a value
    // render in an annoying way.
    if (option.doc().empty()) {
      return option.doc(" ");
    }
    return option;
  }

  template <typename NameValuePair>
  static std::string get_label(const NameValuePair& nvp, PriorityTag<1>,
                               decltype(&NameValuePair::argument_traits) = 0) {
    return nvp.argument_traits().label ?
        nvp.argument_traits().label : "arg";
  }

  template <typename NameValuePair>
  static std::string get_label(const NameValuePair&, PriorityTag<0>) {
    return "arg";
  }

  const std::string prefix_;
  clipp::group group_;
};

}
}
