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

#include "mjlib/telemetry/binary_schema_parser.h"

#include <boost/test/auto_unit_test.hpp>

#include <fmt/format.h>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/test/all_types_struct.h"
#include "mjlib/telemetry/binary_write_archive.h"

using namespace mjlib;

using DUT = telemetry::BinarySchemaParser;

namespace {
std::string FormatName(const DUT::Element* element) {
  if (element->parent) {
    return FormatName(element->parent) + "." + element->name;
  } else {
    return element->name;
  }
}

std::string FormatElement(const DUT::Element& element) {
  return fmt::format(
      "name={} type={} maybe_fixed_size={} int_size={}\n",
      FormatName(&element), static_cast<uint64_t>(element.type),
      element.maybe_fixed_size,
      element.int_size);
}
}  // namespace

BOOST_AUTO_TEST_CASE(BasicBinarySchemaParser) {
  const auto all_types_schema =
      telemetry::BinarySchemaArchive::schema<base::test::AllTypesTest>();
  DUT dut{all_types_schema};

  std::ostringstream ostr;
  for (const auto& element : dut.elements()) {
    ostr << FormatElement(element);
  }

  const std::string expected = R"XX(name= type=16 maybe_fixed_size=-1 int_size=-1
name=.value_bool type=2 maybe_fixed_size=1 int_size=-1
name=.value_i8 type=3 maybe_fixed_size=1 int_size=1
name=.value_i16 type=3 maybe_fixed_size=2 int_size=2
name=.value_i32 type=3 maybe_fixed_size=4 int_size=4
name=.value_i64 type=3 maybe_fixed_size=8 int_size=8
name=.value_u8 type=4 maybe_fixed_size=1 int_size=1
name=.value_u16 type=4 maybe_fixed_size=2 int_size=2
name=.value_u32 type=4 maybe_fixed_size=4 int_size=4
name=.value_u64 type=4 maybe_fixed_size=8 int_size=8
name=.value_f32 type=7 maybe_fixed_size=4 int_size=-1
name=.value_f64 type=8 maybe_fixed_size=8 int_size=-1
name=.value_bytes type=9 maybe_fixed_size=-1 int_size=-1
name=.value_str type=10 maybe_fixed_size=-1 int_size=-1
name=.value_object type=16 maybe_fixed_size=4 int_size=-1
name=.value_object.value_u32 type=4 maybe_fixed_size=4 int_size=4
name=.value_enum type=17 maybe_fixed_size=-1 int_size=-1
name=.value_enum.value_enum type=6 maybe_fixed_size=-1 int_size=-1
name=.value_array type=18 maybe_fixed_size=-1 int_size=-1
name=.value_array.value_array type=16 maybe_fixed_size=4 int_size=-1
name=.value_array.value_array.value_u32 type=4 maybe_fixed_size=4 int_size=4
name=.value_optional type=20 maybe_fixed_size=-1 int_size=-1
name=.value_optional.value_optional type=1 maybe_fixed_size=0 int_size=-1
name=.value_optional.value_optional type=3 maybe_fixed_size=4 int_size=4
name=.value_timestamp type=21 maybe_fixed_size=8 int_size=-1
name=.value_duration type=22 maybe_fixed_size=8 int_size=-1
)XX";
  BOOST_TEST(ostr.str() == expected);
}

namespace {
std::string FormatBytes(const std::string& str) {
  std::ostringstream ostr;
  ostr << "b'";
  for (char c : str) {
    ostr << fmt::format("{:02x}", static_cast<int>(static_cast<uint8_t>(c)));
  }
  ostr << "'";
  return ostr.str();
}

std::string Visit(const DUT::Element* element, base::ReadStream& stream) {
  using FT = telemetry::Format::Type;
  return fmt::format(
      "{} type={} value={}\n",
      FormatName(element), static_cast<int>(element->type),
      [&]() -> std::string {
        switch (element->type) {
          case FT::kMap:
          case FT::kFinal: {
            base::AssertNotReached();
          }
          case FT::kNull: {
            return "null";
          }
          case FT::kBoolean: {
            return fmt::format("{}", element->ReadBoolean(stream));
          }
          case FT::kFixedInt:
          case FT::kVarint: {
            return fmt::format("{}", element->ReadIntLike(stream));
          }
          case FT::kFixedUInt:
          case FT::kVaruint: {
            return fmt::format("{}", element->ReadUIntLike(stream));
          }
          case FT::kFloat32:
          case FT::kFloat64: {
            return fmt::format("{}", element->ReadFloatLike(stream));
          }
          case FT::kBytes:
          case FT::kString: {
            return FormatBytes(element->ReadString(stream));
          }
          case FT::kEnum: {
            return fmt::format("{}", element->ReadUIntLike(stream));
          }
          case FT::kArray: {
            const auto size = element->ReadArraySize(stream);
            std::ostringstream ostr;
            ostr << "[\n";
            for (uint64_t i = 0; i < size; i++) {
              ostr << Visit(element->children.front(), stream);
            }
            ostr << "]";
            return ostr.str();
          }
          case FT::kUnion: {
            const auto union_index = element->ReadUnionIndex(stream);
            return Visit(element->children[union_index], stream);
          }
          case FT::kTimestamp:
          case FT::kDuration: {
            return fmt::format("{}", element->ReadIntLike(stream));
          }
          case FT::kObject: {
            std::ostringstream ostr;
            ostr << "\n";
            for (const auto& field : element->fields) {
              ostr << Visit(field.element, stream);
            }
            return ostr.str();
          }
        }
        base::AssertNotReached();
      }());
}
}

BOOST_AUTO_TEST_CASE(BinarySchemaParserData) {
  const auto all_types_schema =
      telemetry::BinarySchemaArchive::schema<base::test::AllTypesTest>();
  DUT dut(all_types_schema, "");

  base::test::AllTypesTest all_types;
  base::FastOStringStream ostr;
  telemetry::BinaryWriteArchive(ostr).Accept(&all_types);

  std::string str = ostr.str();
  {
    base::BufferReadStream data_stream{str};

    const auto result = Visit(dut.root(), data_stream);
    const std::string expected = R"XX( type=16 value=
.value_bool type=2 value=false
.value_i8 type=3 value=-1
.value_i16 type=3 value=-2
.value_i32 type=3 value=-3
.value_i64 type=3 value=-4
.value_u8 type=4 value=5
.value_u16 type=4 value=6
.value_u32 type=4 value=7
.value_u64 type=4 value=8
.value_f32 type=7 value=9
.value_f64 type=8 value=10
.value_bytes type=9 value=b'0b0c0d'
.value_str type=10 value=b'6465'
.value_object type=16 value=
.value_object.value_u32 type=4 value=3

.value_enum type=17 value=0
.value_array type=18 value=[
.value_array.value_array type=16 value=
.value_array.value_array.value_u32 type=4 value=3

]
.value_optional type=20 value=.value_optional.value_optional type=3 value=21

.value_timestamp type=21 value=1000000
.value_duration type=22 value=500000

)XX";
    BOOST_TEST(result == expected);
  }
  {
    base::BufferReadStream data_stream{str};
    dut.root()->Ignore(data_stream);
    BOOST_TEST(data_stream.remaining() == 0);
  }
}
