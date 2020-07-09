// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/telemetry/emit_json.h"

#include <boost/beast/core/detail/base64.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "mjlib/base/escape_json_string.h"
#include "mjlib/base/time_conversions.h"
#include "mjlib/base/fail.h"
#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

using FT = Format::Type;
using Element = BinarySchemaParser::Element;

namespace {
void WriteArray(std::ostream& ostr, const Element* schema,
                base::ReadStream& data, uint64_t array_size) {
  ostr << "[";
  for (uint64_t i = 0; i < array_size; i++) {
    EmitJson(ostr, schema->children.at(0), data);
    if (i + 1 != array_size) {
      ostr << ", ";
    }
  }
  ostr << "]";
}
}

void EmitJson(std::ostream& ostr, const Element* schema,
              base::ReadStream& data) {
  switch (schema->type) {
    case FT::kNull : {
      ostr << "null";
      return;
    }
    case FT::kBoolean: {
      ostr << (schema->ReadBoolean(data) ? "true" : "false");
      return;
    }
    case FT::kVarint:
    case FT::kFixedInt: {
      ostr << schema->ReadIntLike(data);
      return;
    }
    case FT::kVaruint:
    case FT::kFixedUInt: {
      ostr << schema->ReadUIntLike(data);
      return;
    }
    case FT::kFloat32:
    case FT::kFloat64: {
      ostr << schema->ReadFloatLike(data);
      return;
    }
    case FT::kBytes: {
      const auto raw_bytes = schema->ReadString(data);
      std::vector<char> base64;
      base64.resize(boost::beast::detail::base64::encoded_size(raw_bytes.size()));
      boost::beast::detail::base64::encode(
          &base64[0], &raw_bytes[0], raw_bytes.size());
      ostr << "\"" << std::string(&base64[0], base64.size()) << "\"";

      return;
    }
    case FT::kString: {
      ostr << "\"" << base::EscapeJsonString(schema->ReadString(data)) << "\"";
      return;
    }
    case FT::kObject: {
      ostr << "{";

      for (size_t i = 0; i < schema->fields.size(); i++) {
        ostr << "\"" << schema->fields[i].name << "\" : ";
        EmitJson(ostr, schema->fields[i].element, data);

        if (i + 1 != schema->fields.size()) {
          ostr << ", ";
        }
      }

      ostr << "}";
      return;
    }
    case FT::kEnum: {
      const auto enum_index = schema->ReadUIntLike(data);
      if (schema->enum_items.count(enum_index)) {
        ostr << "\"" << schema->enum_items.at(enum_index) << "\"";
      } else {
        ostr << "\"" << enum_index << "\"";
      }
      return;
    }
    case FT::kArray: {
      const auto array_size = schema->ReadArraySize(data);

      WriteArray(ostr, schema, data, array_size);
      return;
    }
    case FT::kFixedArray: {
      WriteArray(ostr, schema, data, schema->array_size);
      return;
    }
    case FT::kMap: {
      // TODO
      return;
    }
    case FT::kUnion: {
      const auto index = schema->ReadUnionIndex(data);
      EmitJson(ostr, schema->children.at(index), data);
      return;
    }
    case FT::kTimestamp: {
      const auto us_since_epoch = schema->ReadIntLike(data);
      const auto time = base::ConvertEpochMicrosecondsToPtime(us_since_epoch);
      ostr << "\"" << time << "\"";
      return;
    }
    case FT::kDuration: {
      const auto us = schema->ReadIntLike(data);
      const auto time = base::ConvertMicrosecondsToDuration(us);
      ostr << "\"" << time << "\"";
      return;
    }
    case FT::kFinal: {
      base::AssertNotReached();
    }
  }
}

}
}
