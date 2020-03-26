// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/visitor.h"
#include "mjlib/base/visit_archive.h"

#include "mjlib/base/priority_tag.h"
#include "mjlib/base/program_options_archive_detail.h"

namespace mjlib {
namespace base {

class ProgramOptionsArchive : public VisitArchive<ProgramOptionsArchive> {
 public:
  ProgramOptionsArchive(
      boost::program_options::options_description* description,
      std::string prefix = "")
      : description_(description),
        prefix_(prefix) {}

  template <typename NameValuePair>
  auto VisitSerializable(const NameValuePair& pair) {
    ProgramOptionsArchive sub(description_, prefix_ + pair.name() + ".");
    sub.Accept(pair.value());
  }

  template <typename NameValuePair>
  void VisitEnumeration(const NameValuePair& pair) {
    (*description_).add_options()(
        (prefix_ + pair.name()).c_str(),
        new detail::ProgramOptionsEnumArchiveValue<NameValuePair>(pair));
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    ProgramOptionsArchive sub(description_, prefix_ + pair.name() + ".");
    auto* const value = pair.value();
    for (size_t i = 0; i < value->size(); i++) {
      const auto str = std::to_string(i);
      mjlib::base::ReferenceNameValuePair sub_pair(&(*value)[i], str.c_str());
      sub.Visit(sub_pair);
    }
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    VisitOptions(pair, pair.value(), PriorityTag<1>());
  }

  template <typename NameValuePair, typename T>
  void VisitOptions(const NameValuePair&,
                    std::vector<T>*,
                    PriorityTag<1>) {
    // Ignore vectors.
  }

  template <typename NameValuePair, typename T>
  void VisitOptions(const NameValuePair& pair, T*, PriorityTag<0>) {
    (*description_).add_options()(
        (prefix_ + pair.name()).c_str(),
        new detail::ProgramOptionsArchiveValue<NameValuePair>(pair));
  }

  boost::program_options::options_description* options() {
    return description_;
  }

  boost::program_options::options_description* const description_;
  const std::string prefix_;
};

}
}
