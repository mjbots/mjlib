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

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/bytes.h"
#include "mjlib/base/priority_tag.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/visit_archive.h"

#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// This can read a serialized structure assuming that the schema
/// exactly matches what was serialized.  An out of band mechanism is
/// required to enforce this.
class BinaryReadArchive : public base::VisitArchive<BinaryReadArchive> {
 public:
  BinaryReadArchive(base::ReadStream& stream) : stream_(stream) {}

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    VisitHelper(nvp, nvp.value(), base::PriorityTag<2>());
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    Accept(nvp.value());
  }

  template <typename ValueType>
  BinaryReadArchive& Value(ValueType* value) {
    base::ReferenceNameValuePair nvp(value, "");
    VisitArchive<BinaryReadArchive>::Visit(nvp);
    return *this;
  }

  template <typename ValueType>
  static ValueType Read(base::ReadStream& stream) {
    ValueType result;
    BinaryReadArchive(stream).Value(&result);
    return result;
  }

  template <typename ValueType>
  static ValueType Read(std::string_view data) {
    base::BufferReadStream stream(data);
    return Read<ValueType>(stream);
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp,
                        NameMapGetter enumeration_mapper) {
    const auto maybe_value = stream_.ReadVaruint();
    if (!maybe_value) {
      error_ = true;
      return;
    }
    nvp.set_value(static_cast<decltype(enumeration_mapper().begin()->first)>(
                      *maybe_value));
  }

  bool error() const {
    return error_;
  }

 private:
  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   base::Bytes* value,
                   base::PriorityTag<2>) {
    const auto maybe_size = stream_.ReadVaruint();
    if (!maybe_size) {
      error_ = true;
      return;
    }
    const auto size = *maybe_size;
    value->resize(size);
    const bool valid =
        stream_.RawRead(reinterpret_cast<char*>(&(*value)[0]), size);
    if (!valid) { error_ = true; }
  }

  template <typename NameValuePair, typename T, std::size_t N>
  void VisitHelper(const NameValuePair&,
                   std::array<T, N>* value,
                   base::PriorityTag<1>) {
    VisitArrayHelper(*value);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::vector<T>* value,
                   base::PriorityTag<1>) {
    const auto maybe_size = stream_.ReadVaruint();
    if (!maybe_size) {
      error_ = true;
      return;
    }
    const auto size = *maybe_size;
    value->resize(size);
    VisitArrayHelper(*value);
  }

  template <typename Array>
  void VisitArrayHelper(Array& value) {
    for (auto& item : value) {
      base::ReferenceNameValuePair sub_nvp(&item, "");
      Visit(sub_nvp);
      if (error_) { return; }
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::optional<T>* value,
                   base::PriorityTag<1>) {
    const auto maybe_present = stream_.Read<uint8_t>();
    if (!maybe_present) {
      error_ = true;
      return;
    }
    const auto present = *maybe_present;
    if (present == 0) {
      // nothing
    } else if (present == 1) {
      T item = {};
      base::ReferenceNameValuePair sub_nvp(&item, "");
      Visit(sub_nvp);
      *value = item;
    } else {
      error_ = true;
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::ptime*,
                   base::PriorityTag<1>) {
    const auto maybe_data = stream_.Read<int64_t>();
    if (!maybe_data) {
      error_ = true;
      return;
    }
    nvp.set_value(base::ConvertEpochMicrosecondsToPtime(*maybe_data));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::time_duration*,
                   base::PriorityTag<1>) {
    const auto maybe_data = stream_.Read<int64_t>();
    if (!maybe_data) {
      error_ = true;
      return;
    }
    nvp.set_value(base::ConvertMicrosecondsToDuration(*maybe_data));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   std::string*,
                   base::PriorityTag<1>) {
    const auto maybe_data = stream_.ReadString();
    if (!maybe_data) {
      error_ = true;
      return;
    }
    nvp.set_value(*maybe_data);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp,
                   T*,
                   base::PriorityTag<0>) {
    const auto maybe_value = stream_.Read<T>();
    if (!maybe_value) {
      error_ = true;
      return;
    }
    nvp.set_value(*maybe_value);
  }

  ReadStream stream_;
  bool error_ = false;
};

}
}
