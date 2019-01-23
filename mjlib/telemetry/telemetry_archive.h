// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include <optional>
#include <type_traits>

#include "mjlib/base/fast_stream.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/visit_archive.h"

#include "mjlib/telemetry/telemetry_archive_detail.h"
#include "mjlib/telemetry/telemetry_format.h"

namespace mjlib {
namespace telemetry {

/// Generate schema and binary records according to the structure
/// defined in telemetry_format.h.
template <typename RootSerializable>
class TelemetryWriteArchive {
 public:
  typedef TelemetryFormat TF;

  TelemetryWriteArchive() {}

  static std::string schema() { return MakeSchema(); }

  static void Serialize(const RootSerializable* serializable,
                        base::WriteStream& stream_in) {
    TelemetryWriteStream stream(stream_in);

    DataVisitor visitor(stream);
    visitor.Accept(const_cast<RootSerializable*>(serializable));
  }

  static std::string Serialize(const RootSerializable* serializable) {
    base::FastOStringStream ostr;
    Serialize(serializable, ostr);
    return ostr.str();
  }

  static void WriteSchema(base::WriteStream& stream_in) {
    TelemetryWriteStream stream(stream_in);
    stream.Write(static_cast<uint32_t>(0)); // SchemaFlags

    TelemetryWriteArchive::WriteSchemaObject(
        stream, static_cast<RootSerializable*>(0));
  }

  static std::string MakeSchema() {
    base::FastOStringStream ostr;
    WriteSchema(ostr);
    return ostr.str();
  }

 private:
  template <typename Serializable>
  static void WriteSchemaObject(TelemetryWriteStream& stream,
                                Serializable* serializable) {
    SchemaVisitor visitor(stream);
    visitor.Accept(serializable);

    visitor.Finish();
  }

  class SchemaVisitor : public base::VisitArchive<SchemaVisitor> {
   public:
    SchemaVisitor(TelemetryWriteStream& stream) : stream_(stream) {
      stream.Write(static_cast<uint32_t>(0)); // ObjectFlags
    }

    void Finish() {
      // Write out the "final" record.
      stream_.Write(static_cast<uint32_t>(0));
      stream_.WriteString("");
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kFinal));
    }

    template <typename NameValuePair>
    void Visit(const NameValuePair& pair) {
      stream_.Write(static_cast<uint32_t>(0)); // FieldFlags;
      stream_.WriteString(pair.name());

      base::VisitArchive<SchemaVisitor>::Visit(pair);
    }

    template <typename NameValuePair>
    void VisitSerializable(const NameValuePair& pair) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kObject));

      SchemaVisitor visitor(stream_);
      visitor.Accept(pair.value());
      visitor.Finish();
    }

    template <typename NameValuePair>
    void VisitEnumeration(const NameValuePair& pair) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kEnum));

      const auto items = pair.enumeration_mapper();
      uint32_t nvalues = items.size();
      stream_.Write(nvalues);

      for (const auto& pair: items) {
        uint32_t key = static_cast<uint32_t>(pair.first);
        stream_.Write(key);
        stream_.WriteString(pair.second);
      }
    }

    template <typename NameValuePair>
    void VisitScalar(const NameValuePair& pair) {
      VisitHelper(pair, pair.value(), 0);
    }

   private:
    template <typename NameValuePair, typename First, typename Second>
    void VisitHelper(const NameValuePair& pair,
                     std::pair<First, Second>*,
                     int) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kPair));

      base::VisitArchive<SchemaVisitor>::Visit(
          detail::FakeNvp<First>(&pair.value()->first));
      base::VisitArchive<SchemaVisitor>::Visit(
          detail::FakeNvp<Second>(&pair.value()->second));
    }

    template <typename NameValuePair, typename T, std::size_t N>
    void VisitHelper(const NameValuePair&,
                     std::array<T, N>*,
                     int) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kArray));
      stream_.Write(static_cast<uint32_t>(N));

      base::VisitArchive<SchemaVisitor>::Visit(
          detail::FakeNvp<T>(static_cast<T*>(nullptr)));
    }

    template <typename NameValuePair, typename T>
    void VisitHelper(const NameValuePair&,
                     std::vector<T>*,
                     int) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kVector));

      base::VisitArchive<SchemaVisitor>::Visit(
          detail::FakeNvp<T>(static_cast<T*>(nullptr)));
    }

    template <typename NameValuePair, typename T>
    void VisitHelper(const NameValuePair&,
                     std::optional<T>*,
                     int) {
      stream_.Write(static_cast<uint32_t>(TF::FieldType::kOptional));

      base::VisitArchive<SchemaVisitor>::Visit(
          detail::FakeNvp<T>(static_cast<T*>(0)));
    }

    template <typename NameValuePair, typename T>
    void VisitHelper(const NameValuePair& pair,
                     T*,
                     long) {
      VisitPrimitive(pair);
    }

    template <typename NameValuePair>
    void VisitPrimitive(const NameValuePair& pair) {
      stream_.Write(static_cast<uint32_t>(FindType(pair.value())));
    }


    TelemetryWriteStream& stream_;
  };

  class DataVisitor :
      public detail::DataVisitorBase<DataVisitor, TelemetryWriteStream> {
   public:
    typedef detail::DataVisitorBase<DataVisitor, TelemetryWriteStream> Base;
    DataVisitor(TelemetryWriteStream& stream) : Base(stream) {}

    template <typename NameValuePair>
    void VisitVector(const NameValuePair& pair) {
      auto value = pair.value();
      this->stream_.Write(static_cast<uint32_t>(value->size()));
      for (int i = 0; i < static_cast<int>(value->size()); i++) {
        Base::Visit(detail::MakeFakeNvp(&(*value)[i]));
      }
    }

    template <typename NameValuePair>
    void VisitOptional(const NameValuePair& pair) {
      auto value = pair.value();
      this->stream_.Write(static_cast<uint8_t>(*value ? 1 : 0));
      if (*value) {
        Base::Visit(detail::MakeFakeNvp(&(**value)));
      }
    }

    template <typename NameValuePair>
    void VisitString(const NameValuePair& pair) {
      this->stream_.WriteString(pair.get_value());
    }

    template <typename NameValuePair>
    void VisitPrimitive(const NameValuePair& pair) {
      this->stream_.Write(pair.get_value());
    }
  };

  static TF::FieldType FindType(bool*) { return TF::FieldType::kBool; }
  static TF::FieldType FindType(int8_t*) { return TF::FieldType::kInt8; }
  static TF::FieldType FindType(uint8_t*) { return TF::FieldType::kUInt8; }
  static TF::FieldType FindType(int16_t*) { return TF::FieldType::kInt16; }
  static TF::FieldType FindType(uint16_t*) { return TF::FieldType::kUInt16; }
  static TF::FieldType FindType(int32_t*) { return TF::FieldType::kInt32; }
  static TF::FieldType FindType(uint32_t*) { return TF::FieldType::kUInt32; }
  static TF::FieldType FindType(int64_t*) { return TF::FieldType::kInt64; }
  static TF::FieldType FindType(uint64_t*) { return TF::FieldType::kUInt64; }
  static TF::FieldType FindType(float*) { return TF::FieldType::kFloat32; }
  static TF::FieldType FindType(double*) { return TF::FieldType::kFloat64; }
  static TF::FieldType FindType(std::string*) { return TF::FieldType::kString; }
};

/// This archive can read a serialized structure assuming that the
/// schema exactly matches what was serialized.  An out of band
/// mechanism is required to enforce this.
template <typename RootSerializable>
class TelemetrySimpleReadArchive {
 public:
  typedef TelemetryFormat TF;

  static void Deserialize(RootSerializable* serializable,
                          base::ReadStream& stream_in) {
    TelemetryReadStream stream(stream_in);

    DataVisitor visitor(stream);
    visitor.Accept(serializable);
  }

 private:
  class DataVisitor :
      public detail::DataVisitorBase<DataVisitor, TelemetryReadStream> {
   public:
    typedef detail::DataVisitorBase<DataVisitor, TelemetryReadStream> Base;
    DataVisitor(TelemetryReadStream& stream) : Base(stream) {}

    template <typename NameValuePair>
    void VisitVector(const NameValuePair& pair) {
      auto value = pair.value();
      uint32_t size = this->stream_.template Read<uint32_t>();
      value->resize(size);
      for (int i = 0; i < static_cast<int>(value->size()); i++) {
        Base::Visit(detail::MakeFakeNvp(&(*value)[i]));
      }
    }

    template <typename NameValuePair>
    void VisitOptional(const NameValuePair& pair) {
      auto value = pair.value();
      uint8_t present = this->stream_.template Read<uint8_t>();
      if (!present) {
        *value = {};
      } else {
        typedef typename std::decay<decltype(pair.get_value())>::type::value_type ValueType;
        *value = ValueType();
        Base::Visit(detail::MakeFakeNvp(&(**value)));
      }
    }

    template <typename NameValuePair>
    void VisitString(const NameValuePair& pair) {
      pair.set_value(this->stream_.ReadString());
    }

    template <typename NameValuePair>
    void VisitPrimitive(const NameValuePair& pair) {
      typedef typename std::decay<decltype(pair.get_value())>::type T;
      pair.set_value(this->stream_.template Read<T>());
    }
  };
};
}
}
