// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include <fmt/format.h>

#include "mjlib/base/assert.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/base/recording_stream.h"

#include "mjlib/telemetry/telemetry_format.h"

namespace mjlib {
namespace telemetry {

/// Produce a textual representation of a telemetry schema and
/// optionally a data image.
class TelemetrySchemaReader {
 public:
  typedef TelemetryFormat TF;

  TelemetrySchemaReader(TelemetryReadStream& stream,
                        TelemetryReadStream* data,
                        base::WriteStream& out)
      : stream_(stream),
        data_(data),
        out_(out) {}

  void Read() {
    uint32_t flags = stream_.Read<uint32_t>();
    Write(fmt::format("[SchemaFlags]: {:08X};\n", flags));

    ReadSchemaObject(0, kAllInfo);
    Write("\n");
  }

 private:
  enum Semicolon {
    kSemicolon,
    kSkipSemicolon,
  };

  enum DataPolicy {
    kAllInfo,
    kNamesAndData,
    kTypesOnly,
    kDataOnly,
  };

  static bool DisplayNames(DataPolicy p) {
    switch (p) {
      case kAllInfo: { return true; }
      case kNamesAndData: { return true; }
      case kTypesOnly: { return true; }
      case kDataOnly: { return false; }
    }
    return false;
  }

  static bool DisplayTypes(DataPolicy p) {
    switch (p) {
      case kAllInfo: { return true; }
      case kNamesAndData: { return false; }
      case kTypesOnly: { return true; }
      case kDataOnly: { return false; }
    }
    return false;
  }

  static bool DisplayData(DataPolicy p) {
    switch (p) {
      case kAllInfo: { return true; }
      case kNamesAndData: { return true; }
      case kTypesOnly: { return false; }
      case kDataOnly: { return true; }
    }
    return false;
  }

  void DoPrimitiveData(TF::FieldType type) {
    MJ_ASSERT(data_);

    typedef TF::FieldType FT;

    switch (type) {
      case FT::kBool: {
        WriteScalar(data_->Read<bool>());
        break;
      }
      case FT::kInt8: {
        WriteScalar(static_cast<int>(data_->Read<int8_t>()));
        break;
      }
      case FT::kUInt8: {
        WriteScalar(static_cast<int>(data_->Read<uint8_t>()));
        break;
      }
      case FT::kInt16: { WriteScalar(data_->Read<int16_t>()); break; }
      case FT::kUInt16: { WriteScalar(data_->Read<uint16_t>()); break; }
      case FT::kInt32: { WriteScalar(data_->Read<int32_t>()); break; }
      case FT::kUInt32: { WriteScalar(data_->Read<uint32_t>()); break; }
      case FT::kInt64: { WriteScalar(data_->Read<int64_t>()); break; }
      case FT::kUInt64: { WriteScalar(data_->Read<uint64_t>()); break; }
      case FT::kFloat32: { WriteScalar(data_->Read<float>()); break; }
      case FT::kFloat64: { WriteScalar(data_->Read<double>()); break; }
      case FT::kString: {
        Write("\"" + data_->ReadString() + "\"");
        break;
      }
      case FT::kPtime: {
        MJ_ASSERT(false);
        break;
      }
      case FT::kFinal:
      case FT::kPair:
      case FT::kArray:
      case FT::kVector:
      case FT::kObject:
      case FT::kOptional:
      case FT::kEnum: {
        MJ_ASSERT(false);
      }
    }
  }

  void RenderVariableSize(int indent, uint32_t num_elements,
                          DataPolicy data_policy) {
    base::RecordingStream io_stream(stream_.stream());
    TelemetryReadStream recording_stream(io_stream);
    TelemetrySchemaReader sub_reader(recording_stream, nullptr, out_);
    sub_reader.DoField(indent + 2, 0, "", kSkipSemicolon, data_policy);

    if (DisplayData(data_policy) && data_) {
      if (DisplayNames(data_policy)) {
        Write(" = ");
      }
      Write("[");
      for (uint32_t i = 0; i < num_elements; i++) {
        base::FastIStringStream istr(io_stream.str());
        TelemetryReadStream sub_stream(istr);
        TelemetrySchemaReader field_reader(sub_stream, data_, out_);
        field_reader.DoField(indent + 2, 0, "", kSkipSemicolon, kDataOnly);
        if ((i + 1) != num_elements) { Write(","); }
      }
      Write("]");
    }
  }

  bool DoField(int indent,
               uint32_t field_flags, const std::string& field_name,
               Semicolon final_semicolon,
               DataPolicy data_policy) {
    std::string instr = std::string(indent, ' ');

    TF::FieldType field_type = static_cast<TF::FieldType>(
        stream_.Read<uint32_t>());

    if (field_type == TF::FieldType::kFinal) {
      return true;
    }

    std::string flags_str;
    if (field_flags) {
      flags_str = fmt::format(" f={:08X}", field_flags);
    }
    if (DisplayNames(data_policy)) {
      Write(fmt::format("{}{}", instr, field_name));
    }

    if (DisplayTypes(data_policy)) {
      Write(fmt::format(": {}{}", FieldTypeStr(field_type), flags_str));
    }

    switch (field_type) {
      case TF::FieldType::kFinal: {
        MJ_ASSERT(false);
      }
      case TF::FieldType::kBool:
      case TF::FieldType::kInt8:
      case TF::FieldType::kUInt8:
      case TF::FieldType::kInt16:
      case TF::FieldType::kUInt16:
      case TF::FieldType::kInt32:
      case TF::FieldType::kUInt32:
      case TF::FieldType::kInt64:
      case TF::FieldType::kUInt64:
      case TF::FieldType::kFloat32:
      case TF::FieldType::kFloat64:
      case TF::FieldType::kPtime:
      case TF::FieldType::kString: {
        if (data_) {
          if (DisplayNames(data_policy)) {
            Write(" = ");
          }
          if (DisplayData(data_policy)) {
            DoPrimitiveData(field_type);
          }
        }
        break;
      }
      case TF::FieldType::kPair: {
        Write("<\n");
        DoField(indent + 2, 0, "", kSkipSemicolon, data_policy);
        Write("\n");
        DoField(indent + 2, 0, "", kSkipSemicolon, data_policy);
        Write("\n" + instr + "  >");
        break;
      }
      case TF::FieldType::kArray: {
        uint32_t nelements = stream_.Read<uint32_t>();
        if (nelements >
            static_cast<uint32_t>(TF::BlockOffsets::kMaxBlockSize)) {
          throw std::runtime_error("corrupt array size");
        }
        Write(fmt::format("[{}]\n", nelements));

        RenderVariableSize(indent, nelements, data_policy);

        break;
      }
      case TF::FieldType::kVector: {
        if (DisplayNames(data_policy)) {
          Write("\n");
        }

        uint32_t nelements = 0;
        if (data_) {
          nelements = data_->Read<uint32_t>();
        }

        RenderVariableSize(indent, nelements, data_policy);

        break;
      }
      case TF::FieldType::kObject: {
        Write("\n");
        ReadSchemaObject(indent + 2, data_policy);
        break;
      }
      case TF::FieldType::kOptional: {
        if (DisplayNames(data_policy)) {
          Write("\n");
        }

        uint8_t present = 0;
        if (data_) {
          present = data_->Read<uint8_t>();
        }

        RenderVariableSize(indent, present, data_policy);
        break;
      }
      case TF::FieldType::kEnum: {
        uint32_t nvalues = stream_.Read<uint32_t>();
        std::map<int, std::string> items;
        for (uint32_t i = 0; i < nvalues; i++) {
          uint32_t key = stream_.Read<uint32_t>();
          std::string value = stream_.ReadString();
          items.insert(std::make_pair(key, value));
        }
        if (data_) {
          uint32_t value = data_->Read<uint32_t>();
          if (DisplayNames(data_policy)) {
            Write(" = ");
          }
          if (DisplayData(data_policy)) {
            if (items.count(value)) {
              Write(fmt::format("{} ({})", items[value], value));
            } else {
              WriteScalar(value);
            }
          }
        }
        break;
      }
      default: {
        MJ_ASSERT(false);
      }
    }

    if (data_policy == kAllInfo) {
      switch (final_semicolon) {
        case kSemicolon: { Write(";\n"); break; }
        case kSkipSemicolon: { break; }
      }
    }
    return false;
  }

  void ReadSchemaObject(int indent_in, DataPolicy data_policy) {
    Write(fmt::format("{}{{", std::string(indent_in, ' ')));
    if (DisplayNames(data_policy)) { Write("\n"); }

    int indent = indent_in + 2;
    std::string instr = std::string(indent, ' ');
    uint32_t flags = stream_.Read<uint32_t>();
    if (data_policy != kDataOnly && flags != 0) {
      Write(fmt::format("{}[Flags]: {:08X};\n", instr, flags));
    }
    bool done = false;
    while (!done) {
      uint32_t field_flags = stream_.Read<uint32_t>();
      std::string field_name = stream_.ReadString();

      const bool final = DoField(
          indent, field_flags, field_name, kSemicolon, data_policy);
      if (final) { done = true; }
    }

    if (DisplayNames(data_policy)) {
      Write(fmt::format("{}", std::string(indent_in, ' ')));
    }
    Write("}");
  }

  static std::string FieldTypeStr(TF::FieldType v) {
    switch (v) {
      case TF::FieldType::kFinal: { return "kFinal"; }
      case TF::FieldType::kBool: { return "kBool"; }
      case TF::FieldType::kInt8: { return "kInt8"; }
      case TF::FieldType::kUInt8: { return "kUInt8"; }
      case TF::FieldType::kInt16: { return "kInt16"; }
      case TF::FieldType::kUInt16: { return "kUInt16"; }
      case TF::FieldType::kInt32: { return "kInt32"; }
      case TF::FieldType::kUInt32: { return "kUInt32"; }
      case TF::FieldType::kInt64: { return "kInt64"; }
      case TF::FieldType::kUInt64: { return "kUInt64"; }
      case TF::FieldType::kFloat32: { return "kFloat32"; }
      case TF::FieldType::kFloat64: { return "kFloat64"; }
      case TF::FieldType::kPtime: { return "kPtime"; }
      case TF::FieldType::kString: { return "kString"; }
      case TF::FieldType::kPair: { return "kPair"; }
      case TF::FieldType::kArray: { return "kArray"; }
      case TF::FieldType::kVector: { return "kVector"; }
      case TF::FieldType::kObject: { return "kObject"; }
      case TF::FieldType::kOptional: { return "kOptional"; }
      case TF::FieldType::kEnum: { return "kEnum"; }
    }
    MJ_ASSERT(false);
    return "";
  }

  void Write(const std::string& data) {
    out_.write({&data[0], data.size()});
  }

  template <typename T>
  void WriteScalar(const T& value) {
    Write(fmt::format("{}", value));
  }

  TelemetryReadStream& stream_;
  TelemetryReadStream* const data_;
  base::WriteStream& out_;
};

}
}
