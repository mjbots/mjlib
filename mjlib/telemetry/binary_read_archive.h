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

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

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

  template <typename NameValuePair>
  void VisitEnumeration(const NameValuePair& nvp) {
    nvp.set_value(stream_.ReadVaruint());
  }

 private:
  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   base::Bytes* value,
                   base::PriorityTag<2>) {
    const auto size = stream_.ReadVaruint();
    value->resize(size);
    stream_.RawRead(reinterpret_cast<char*>(&(*value)[0]), size);
  }

  template <typename NameValuePair, typename T, std::size_t N>
  void VisitHelper(const NameValuePair&,
                   std::array<T, N>* value,
                   base::PriorityTag<1>) {
    const auto size = stream_.ReadVaruint();
    if (size != N) {
      // TODO jpieper: Add exception.
      MJ_ASSERT(false);
    }
    VisitArray(*value);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::vector<T>* value,
                   base::PriorityTag<1>) {
    const auto size = stream_.ReadVaruint();
    value->resize(size);
    VisitArray(*value);
  }

  template <typename Array>
  void VisitArray(Array& value) {
    for (auto& item : value) {
      base::ReferenceNameValuePair sub_nvp(&item, "");
      Visit(sub_nvp);
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::optional<T>* value,
                   base::PriorityTag<1>) {
    const uint8_t present = stream_.Read<uint8_t>();
    if (present == 0) {
      // nothing
    } else if (present == 1) {
      T item = {};
      base::ReferenceNameValuePair sub_nvp(&item, "");
      Visit(sub_nvp);
      *value = item;
    } else {
      MJ_ASSERT(false);
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::ptime*,
                   base::PriorityTag<1>) {
    nvp.set_value(
        base::ConvertEpochMicrosecondsToPtime(stream_.Read<int64_t>()));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::time_duration*,
                   base::PriorityTag<1>) {
    nvp.set_value(
        base::ConvertMicrosecondsToDuration(stream_.Read<int64_t>()));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   std::string*,
                   base::PriorityTag<1>) {
    nvp.set_value(stream_.ReadString());
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp,
                   T*,
                   base::PriorityTag<0>) {
    nvp.set_value(stream_.Read<T>());
  }

  ReadStream stream_;
};

}
}
