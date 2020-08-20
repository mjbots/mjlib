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
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/base/bytes.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/base/priority_tag.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/visit_archive.h"

#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// Emit a binary data serialization from a serializable C++ object.
class BinaryWriteArchive : public base::VisitArchive<BinaryWriteArchive> {
 public:
  BinaryWriteArchive(base::WriteStream& stream) : stream_(stream) {}

  template <typename Serializable>
  static std::string Write(const Serializable* serializable) {
    base::FastOStringStream ostr;
    BinaryWriteArchive(ostr).Accept(serializable);
    return ostr.str();
  }

  template <typename Serializable>
  BinaryWriteArchive& Accept(const Serializable* serializable) {
    base::VisitArchive<BinaryWriteArchive>::Accept(
        const_cast<Serializable*>(serializable));
    return *this;
  }

  template <typename ValueType>
  BinaryWriteArchive& Value(const ValueType& value) {
    base::ReferenceNameValuePair nvp(const_cast<ValueType*>(&value), "");
    VisitArchive<BinaryWriteArchive>::Visit(nvp);
    return *this;
  }

  template <typename ValueType>
  static void Write(const ValueType& value, base::WriteStream& stream) {
    BinaryWriteArchive(stream).Value(value);
  }

  template <typename ValueType>
  static std::string Write(const ValueType& value) {
    base::FastOStringStream ostr;
    Write(value, ostr);
    return ostr.str();
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    VisitHelper(nvp, nvp.value(), base::PriorityTag<2>());
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    Accept(nvp.value());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp, NameMapGetter) {
    stream_.WriteVaruint(static_cast<uint64_t>(nvp.get_value()));
  }

 private:
  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   base::Bytes* value,
                   base::PriorityTag<2>) {
    stream_.WriteVaruint(value->size());
    stream_.RawWrite({
        reinterpret_cast<const char*>(&(*value)[0]), value->size()});
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
    stream_.WriteVaruint(value->size());
    VisitArrayHelper(*value);
  }

  template <typename Array>
  void VisitArrayHelper(Array& value) {
    for (auto& item : value) {
      base::ReferenceNameValuePair sub_nvp(&item, "");
      Visit(sub_nvp);
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::optional<T>* value,
                   base::PriorityTag<1>) {
    if (!(*value)) {
      stream_.WriteVaruint(0);
    } else {
      stream_.WriteVaruint(1);
      base::ReferenceNameValuePair sub_nvp(&**value, "");
      Visit(sub_nvp);
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   boost::posix_time::ptime* value,
                   base::PriorityTag<1>) {
    stream_.Write(base::ConvertPtimeToEpochMicroseconds(*value));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   boost::posix_time::time_duration* value,
                   base::PriorityTag<1>) {
    stream_.Write(base::ConvertDurationToMicroseconds(*value));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   std::string* value,
                   base::PriorityTag<1>) {
    stream_.WriteString(*value);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   T* value,
                   base::PriorityTag<0>) {
    stream_.Write(*value);
  }

  WriteStream stream_;
};

/// Emit a binary schema serialization from a serializable C++ object.
class BinarySchemaArchive : public base::VisitArchive<BinarySchemaArchive> {
 public:
  using TF = Format;

  struct Options {
    // Emit the "default" value associated with each item.
    bool emit_default = true;

    Options() {}
  };

  BinarySchemaArchive(base::WriteStream& stream, const Options& options = {})
      : stream_(stream),
        options_(options) {}

  template <typename Serializable>
  static std::string schema(const Options& options = {}) {
    base::FastOStringStream ostr;
    BinarySchemaArchive archive(ostr, options);
    Serializable serializable;
    archive.Accept(&serializable);
    return ostr.str();
  }

  template <typename Serializable>
  BinarySchemaArchive& Accept(const Serializable* serializable) {
    stream_.WriteVaruint(TF::Type::kObject);
    stream_.WriteVaruint(0);  // ObjectFlags
    base::VisitArchive<BinarySchemaArchive>::Accept(
        const_cast<Serializable*>(serializable));
    Finish();
    return *this;
  }

  template <typename ValueType>
  BinarySchemaArchive& Value() {
    ValueType value;
    base::ReferenceNameValuePair nvp(const_cast<ValueType*>(&value), "");
    VisitArchive<BinarySchemaArchive>::Visit(nvp);
    return *this;
  }

  template <typename ValueType>
  static void Write(base::WriteStream& stream, const Options& options = {}) {
    BinarySchemaArchive(stream, options).template Value<ValueType>();
  }

  template <typename ValueType>
  static std::string Write(const Options& options = {}) {
    base::FastOStringStream ostr;
    Write<ValueType>(ostr, options);
    return ostr.str();
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& nvp) {
    stream_.WriteVaruint(0);  // FieldFlags
    stream_.WriteString(nvp.name());
    stream_.WriteVaruint(0);  // naliases

    base::VisitArchive<BinarySchemaArchive>::Visit(nvp);

    if (options_.emit_default) {
      stream_.WriteVaruint(1);  // default value has data

      // Emit the default value.
      BinaryWriteArchive data_archive(stream_.base());
      data_archive.Visit(nvp);
    } else {
      stream_.WriteVaruint(0);
    }
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    BinarySchemaArchive sub_archive(stream_.base(), options_);
    sub_archive.Accept(nvp.value());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair&,
                        NameMapGetter enumeration_mapper) {
    stream_.WriteVaruint(TF::Type::kEnum);

    /// For now, we only provide a way to have enums of type varuint.
    /// Later we can support the other fixed types.
    stream_.WriteVaruint(TF::Type::kVaruint);

    const auto items = enumeration_mapper();
    const auto nvalues = items.size();
    stream_.WriteVaruint(nvalues);

    for (const auto& pair : items) {
      auto key = static_cast<int64_t>(pair.first);
      stream_.WriteVaruint(key);
      stream_.WriteString(pair.second);
    }
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    VisitHelper(nvp, nvp.value(), base::PriorityTag<2>());
  }

 private:
  void Finish() {
    // Write out the "final" record.
    stream_.WriteVaruint(0);  // FieldFlags
    stream_.WriteString("");
    stream_.WriteVaruint(0);  // naliases
    stream_.WriteVaruint(0);  // kFinal
    stream_.WriteVaruint(0);  // default value: null
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair&,
                   base::Bytes*,
                   base::PriorityTag<2>) {
    stream_.WriteVaruint(TF::Type::kBytes);
  }

  template <typename NameValuePair, typename T, std::size_t N>
  void VisitHelper(const NameValuePair&,
                   std::array<T, N>*,
                   base::PriorityTag<1>) {
    stream_.WriteVaruint(TF::Type::kFixedArray);
    stream_.WriteVaruint(N);
    T item{};
    VisitArrayHelper(item);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::vector<T>*,
                   base::PriorityTag<1>) {
    stream_.WriteVaruint(TF::Type::kArray);

    T item{};
    VisitArrayHelper(item);
  }

  template <typename T>
  void VisitArrayHelper(T& value) {
    base::ReferenceNameValuePair nvp(&value, "");
    base::VisitArchive<BinarySchemaArchive>::Visit(nvp);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::optional<T>*,
                   base::PriorityTag<1>) {
    // Emit this as a union of null and the actual type.
    stream_.WriteVaruint(TF::Type::kUnion);

    stream_.WriteVaruint(TF::Type::kNull);

    T value;
    base::ReferenceNameValuePair nvp(&value, "");
    base::VisitArchive<BinarySchemaArchive>::Visit(nvp);

    stream_.WriteVaruint(TF::Type::kFinal);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   T*,
                   base::PriorityTag<0>) {
    VisitPrimitive(pair);
  }

  template <typename NameValuePair>
  void VisitPrimitive(const NameValuePair& pair) {
    for (auto value : BinarySchemaArchive::FindType(pair.value())) {
      stream_.WriteVaruint(value);
    }
  }

  template <size_t N>
  using TR = std::array<uint64_t, N>;

  template <typename T>
  static uint64_t u64(T value) { return static_cast<uint64_t>(value); }

  static TR<1> FindType(bool*) { return {u64(TF::Type::kBoolean)}; }

  static TR<2> FindType(int8_t*) { return {u64(TF::Type::kFixedInt), 1}; }
  static TR<2> FindType(int16_t*) { return {u64(TF::Type::kFixedInt), 2}; }
  static TR<2> FindType(int32_t*) { return {u64(TF::Type::kFixedInt), 4}; }
  static TR<2> FindType(int64_t*) { return {u64(TF::Type::kFixedInt), 8}; }
  static TR<2> FindType(uint8_t*) { return {u64(TF::Type::kFixedUInt), 1}; }
  static TR<2> FindType(uint16_t*) { return {u64(TF::Type::kFixedUInt), 2}; }
  static TR<2> FindType(uint32_t*) { return {u64(TF::Type::kFixedUInt), 4}; }
  static TR<2> FindType(uint64_t*) { return {u64(TF::Type::kFixedUInt), 8}; }

  static TR<1> FindType(float*) { return {u64(TF::Type::kFloat32)}; }
  static TR<1> FindType(double*) { return {u64(TF::Type::kFloat64)}; }
  static TR<1> FindType(base::Bytes*) { return {u64(TF::Type::kBytes)}; }
  static TR<1> FindType(std::string*) { return {u64(TF::Type::kString)}; }
  static TR<1> FindType(boost::posix_time::ptime*) {
    return {u64(TF::Type::kTimestamp)};
  }
  static TR<1> FindType(boost::posix_time::time_duration*) {
    return {u64(TF::Type::kDuration)};
  }

  WriteStream stream_;
  Options options_;
};

}
}
