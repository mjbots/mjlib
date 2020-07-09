// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <fmt/format.h>

#include "mjlib/base/bytes.h"
#include "mjlib/base/escape_json_string.h"
#include "mjlib/base/priority_tag.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/visit_archive.h"

namespace mjlib {
namespace base {

/// Emit JSON5 from a given serializable.
///
/// https://json5.org
class Json5WriteArchive : public VisitArchive<Json5WriteArchive> {
 public:
  struct Options {
    int indent = 0;

    /// Emit only standard JSON
    bool standard = false;

    Options() {}

    Options& set_indent(int value) {
      indent = value;
      return *this;
    }

    Options& set_standard(bool value) {
      standard = value;
      return *this;
    }
  };

  Json5WriteArchive(std::ostream& stream, const Options& options = Options())
      : stream_(stream),
        options_(options) {}

  template <typename Value>
  static std::string Write(const Value& value,
                           const Options& options = Options()) {
    std::ostringstream ostr;
    Json5WriteArchive(ostr, options).Value(value);
    return ostr.str();
  }

  template <typename Serializable>
  Json5WriteArchive& Accept(Serializable* serializable) {
    stream_ << "{\n";
    VisitArchive<Json5WriteArchive>::Accept(serializable);
    stream_ << "\n" << Indent(-2) << "}";
    return *this;
  }

  template <typename ValueType>
  Json5WriteArchive& Value(const ValueType& value) {
    base::ReferenceNameValuePair nvp(const_cast<ValueType*>(&value), "");
    VisitArchive<Json5WriteArchive>::Visit(nvp);
    return *this;
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& nvp) {
    if (!first_) {
      stream_ << ",\n";
    }
    first_ = false;

    // Write out the field name and colon.
    stream_ << Indent() << "\"" << nvp.name() << "\" : ";

    VisitArchive<Json5WriteArchive>::Visit(nvp);
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    auto new_options = options_;
    Json5WriteArchive sub_archive(
        stream_, new_options.set_indent(options_.indent + 2));
    sub_archive.Accept(nvp.value());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp,
                        NameMapGetter enumeration_mapper) {
    stream_ << "\"" << enumeration_mapper().at(*nvp.value()) << "\"";
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& nvp) {
    WriteArray(*nvp.value());
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    VisitHelper(nvp, nvp.value(), base::PriorityTag<2>());
  }

 private:
  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   Bytes* value,
                   base::PriorityTag<2> tag) {
    VisitHelper(nvp, static_cast<std::vector<uint8_t>*>(value), tag);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::vector<T>* value,
                   base::PriorityTag<1>) {
    WriteArray(*value);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::optional<T>* value,
                   base::PriorityTag<1>) {
    if (!(*value)) {
      stream_ << "null";
    } else {
      Value(**value);
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   boost::posix_time::ptime* value,
                   base::PriorityTag<1>) {
    stream_ << "\"";
    stream_ << boost::posix_time::to_simple_string(*value);
    stream_ << "\"";
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   boost::posix_time::time_duration* value,
                   base::PriorityTag<1>) {
    stream_ << "\"";
    stream_ << boost::posix_time::to_simple_string(*value);
    stream_ << "\"";
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   std::string* value,
                   base::PriorityTag<1>) {
    stream_ << "\"";
    stream_ << EscapeJsonString(*value);
    stream_ << "\"";
  }

  template <typename T>
  void FormatFloat(T value, const char* format_string) {
    if (value == std::numeric_limits<T>::infinity()) {
      stream_ << (options_.standard ? "null" : "Infinity");
    } else if (value == -std::numeric_limits<T>::infinity()) {
      stream_ << (options_.standard ? "null" : "-Infinity");
    } else if (!std::isfinite(value)) {
      stream_ << (options_.standard ? "null" : "NaN");
    } else {
      stream_ << fmt::format(format_string, value);
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   float* value,
                   base::PriorityTag<1>) {
    FormatFloat(*value, "{:.9g}");
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   double* value,
                   base::PriorityTag<1>) {
    FormatFloat(*value, "{:.17g}");
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   bool*,
                   base::PriorityTag<1>) {
    stream_ << (nvp.get_value() ? "true" : "false");
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   T* value,
                   base::PriorityTag<0>) {
    stream_ << fmt::format("{:d}", *value);
  }

  template <typename Array>
  void WriteArray(Array& array) {
    stream_ << "[\n";
    auto options_copy = options_;
    Json5WriteArchive sub_archive(
        stream_, options_copy.set_indent(options_copy.indent + 2));
    bool first = true;
    for (auto& item : array) {
      if (!first) {
        stream_ << ",\n";
      }
      first = false;
      stream_ << Indent(2);
      Value(item);
    }
    stream_ << "\n" << Indent() << "]";
  }

  std::string Indent(int extra = 0) {
    return std::string(options_.indent + extra + 2, ' ');
  }

  std::ostream& stream_;
  const Options options_;

  bool first_ = true;
};

}
}
