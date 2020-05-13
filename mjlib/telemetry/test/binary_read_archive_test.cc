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

#include "mjlib/telemetry/binary_read_archive.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/fast_stream.h"

#include "mjlib/base/test/all_types_struct.h"

using namespace mjlib;

BOOST_AUTO_TEST_CASE(BinaryReadArchive) {
  std::vector<uint8_t> source = {
    0x01,  // value_bool : true
    0xf0,  // value_i8 : -16
    0xf1, 0xff,  // value_i16 : -15
    0xf2, 0xff, 0xff, 0xff,  // value_i32 : -14
    0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // value_i64 : -13
    0x08,  // value_u8 : 8
    0x09, 0x00,  // value_u16 : 9
    0x0a, 0x00, 0x00, 0x00,  // value_u32 : 10
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_u64 : 11
    0x00, 0x00, 0x00, 0x00,  // value_f32 : 0.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_f64 : 0.0
    0x01, 0x08,  // value_bytes : [0x08]
    0x04, 't', 'e', 's', 't',  // value_str : 4
    0x06, 0x00, 0x00, 0x00,  // value_object : SubTest1{6}
    0x05,  // value_enum : kNextValue
    0x00,  // value_array : []
    0x20, 0x21,  // value_fixedarray : [32, 33]
    0x01, 0x09, 0x00, 0x00, 0x00,  // value_optional : 9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_timestamp : 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_duration : 0
  };

  std::string source_str(reinterpret_cast<const char*>(&source[0]), source.size());
  {
    base::FastIStringStream istr(source_str);

    telemetry::BinaryReadArchive dut(istr);
    base::test::AllTypesTest all_types;
    dut.Accept(&all_types);

    BOOST_TEST(all_types.value_bool == true);
    BOOST_TEST(all_types.value_i8 == -16);
    BOOST_TEST(all_types.value_i16 == -15);
    BOOST_TEST(all_types.value_i32 == -14);
    BOOST_TEST(all_types.value_i64 == -13);
    BOOST_TEST(all_types.value_u8 == 8);
    BOOST_TEST(all_types.value_u16 == 9);
    BOOST_TEST(all_types.value_u32 == 10);
    BOOST_TEST(all_types.value_u64 == 11);
    BOOST_TEST(all_types.value_f32 == 0.0);
    BOOST_TEST(all_types.value_f64 == 0.0);
    BOOST_TEST(all_types.value_bytes.size() == 1);
    BOOST_TEST(all_types.value_bytes[0] == 0x08);
    BOOST_TEST(all_types.value_str == "test");
    BOOST_TEST(all_types.value_object.value_u32 == 6);
    BOOST_TEST((all_types.value_enum == base::test::TestEnumeration::kNextValue));
    BOOST_TEST(all_types.value_array.size() == 0);
    BOOST_TEST(all_types.value_fixedarray[0] == 32);
    BOOST_TEST(all_types.value_fixedarray[1] == 33);
    BOOST_TEST(all_types.value_optional.has_value());
    BOOST_TEST(*all_types.value_optional == 9);
    BOOST_TEST(base::ConvertPtimeToEpochMicroseconds(all_types.value_timestamp) == 0);
    BOOST_TEST(base::ConvertDurationToMicroseconds(all_types.value_duration) == 0);
  }
  {
    const auto another_all_types = telemetry::BinaryReadArchive::Read<
      base::test::AllTypesTest>(source_str);
    BOOST_TEST(*another_all_types.value_optional == 9);
  }
}
