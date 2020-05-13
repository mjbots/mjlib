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

#include <sstream>

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

std::string Hexify(const std::string& data) {
  std::ostringstream ostr;
  for (auto c : data) {
    ostr << fmt::format("{:02x}", static_cast<int>(static_cast<uint8_t>(c)));
  }
  return ostr.str();
}

std::string FormatElement(const DUT::Element& element) {
  return fmt::format(
      "name={} type={} bs={} maybe_fixed_size={} int_size={}\n",
      FormatName(&element), static_cast<uint64_t>(element.type),
      Hexify(element.binary_schema),
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

  const std::string expected = R"XX(name= type=16 bs=1000000a76616c75655f626f6f6c00020100000876616c75655f693800030101ff000976616c75655f69313600030201feff000976616c75655f69333200030401fdffffff000976616c75655f69363400030801fcffffffffffffff000876616c75655f75380004010105000976616c75655f753136000402010600000976616c75655f7533320004040107000000000976616c75655f753634000408010800000000000000000976616c75655f66333200070100001041000976616c75655f6636340008010000000000002440000b76616c75655f6279746573000901030b0c0d000976616c75655f737472000a01026465000c76616c75655f6f626a656374001000000976616c75655f753332000404010300000000000000000103000000000a76616c75655f656e756d0011060300076b56616c756531050a6b4e65787456616c7565140d6b416e6f7468657256616c75650100000b76616c75655f617272617900121000000976616c75655f75333200040401030000000000000000010103000000001076616c75655f666978656461727261790013020401010e0f000e76616c75655f6f7074696f6e616c001501030400010115000000000f76616c75655f74696d657374616d7000160140420f0000000000000e76616c75655f6475726174696f6e00170120a10700000000000000000000 maybe_fixed_size=-1 int_size=-1
name=.value_bool type=2 bs=02 maybe_fixed_size=1 int_size=-1
name=.value_i8 type=3 bs=0301 maybe_fixed_size=1 int_size=1
name=.value_i16 type=3 bs=0302 maybe_fixed_size=2 int_size=2
name=.value_i32 type=3 bs=0304 maybe_fixed_size=4 int_size=4
name=.value_i64 type=3 bs=0308 maybe_fixed_size=8 int_size=8
name=.value_u8 type=4 bs=0401 maybe_fixed_size=1 int_size=1
name=.value_u16 type=4 bs=0402 maybe_fixed_size=2 int_size=2
name=.value_u32 type=4 bs=0404 maybe_fixed_size=4 int_size=4
name=.value_u64 type=4 bs=0408 maybe_fixed_size=8 int_size=8
name=.value_f32 type=7 bs=07 maybe_fixed_size=4 int_size=-1
name=.value_f64 type=8 bs=08 maybe_fixed_size=8 int_size=-1
name=.value_bytes type=9 bs=09 maybe_fixed_size=-1 int_size=-1
name=.value_str type=10 bs=0a maybe_fixed_size=-1 int_size=-1
name=.value_object type=16 bs=1000000976616c75655f75333200040401030000000000000000 maybe_fixed_size=4 int_size=-1
name=.value_object.value_u32 type=4 bs=0404 maybe_fixed_size=4 int_size=4
name=.value_enum type=17 bs=11060300076b56616c756531050a6b4e65787456616c7565140d6b416e6f7468657256616c7565 maybe_fixed_size=-1 int_size=-1
name=.value_enum.value_enum type=6 bs=06 maybe_fixed_size=-1 int_size=-1
name=.value_array type=18 bs=121000000976616c75655f75333200040401030000000000000000 maybe_fixed_size=-1 int_size=-1
name=.value_array.value_array type=16 bs=1000000976616c75655f75333200040401030000000000000000 maybe_fixed_size=4 int_size=-1
name=.value_array.value_array.value_u32 type=4 bs=0404 maybe_fixed_size=4 int_size=4
name=.value_fixedarray type=19 bs=13020401 maybe_fixed_size=-1 int_size=-1
name=.value_fixedarray.value_fixedarray type=4 bs=0401 maybe_fixed_size=1 int_size=1
name=.value_optional type=21 bs=1501030400 maybe_fixed_size=-1 int_size=-1
name=.value_optional.value_optional type=1 bs=01 maybe_fixed_size=0 int_size=-1
name=.value_optional.value_optional type=3 bs=0304 maybe_fixed_size=4 int_size=4
name=.value_timestamp type=22 bs=16 maybe_fixed_size=8 int_size=-1
name=.value_duration type=23 bs=17 maybe_fixed_size=8 int_size=-1
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
          case FT::kFixedArray: {
            const auto size = element->array_size;
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
.value_fixedarray type=19 value=[
.value_fixedarray.value_fixedarray type=4 value=14
.value_fixedarray.value_fixedarray type=4 value=15
]
.value_optional type=21 value=.value_optional.value_optional type=3 value=21

.value_timestamp type=22 value=1000000
.value_duration type=23 value=500000

)XX";
    BOOST_TEST(result == expected);
  }
  {
    base::BufferReadStream data_stream{str};
    dut.root()->Ignore(data_stream);
    BOOST_TEST(data_stream.remaining() == 0);
  }
}
