// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/json5_write_archive.h"

#include <sstream>

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/test/all_types_struct.h"

using namespace mjlib;

using DUT = base::Json5WriteArchive;

BOOST_AUTO_TEST_CASE(BasicJson5Write) {
  std::ostringstream ostr;
  base::test::AllTypesTest all_types;
  DUT(ostr).Accept(&all_types);

  const std::string actual = ostr.str();
  const std::string expected = R"XXX({
  "value_bool" : false,
  "value_i8" : -1,
  "value_i16" : -2,
  "value_i32" : -3,
  "value_i64" : -4,
  "value_u8" : 5,
  "value_u16" : 6,
  "value_u32" : 7,
  "value_u64" : 8,
  "value_f32" : 9,
  "value_f64" : 10,
  "value_bytes" : [
    11,
    12,
    13
  ],
  "value_str" : "de",
  "value_object" : {
    "value_u32" : 3
  },
  "value_enum" : "kValue1",
  "value_array" : [
    {
    "value_u32" : 3
  }
  ],
  "value_fixedarray" : [
    14,
    15
  ],
  "value_optional" : 21,
  "value_timestamp" : "1970-Jan-01 00:00:01",
  "value_duration" : "00:00:00.500000"
})XXX";
  BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_CASE(JsonStringEscape) {
  BOOST_TEST(DUT::Write(std::string("abcdef"))
             == "\"abcdef\"");
  BOOST_TEST(DUT::Write(
                 std::string("a\"\\\b\f\n\r\t\x00"
                             "def", 12))
             == "\"a\\\"\\\\\\b\\f\\n\\r\\t\\u0000def\"");
}

BOOST_AUTO_TEST_CASE(JsonSpecialNumber) {
  BOOST_TEST(DUT::Write(
                 std::numeric_limits<double>::infinity()) == "Infinity");
  BOOST_TEST(DUT::Write(
                 -std::numeric_limits<double>::infinity()) == "-Infinity");
  BOOST_TEST(DUT::Write(
                 std::numeric_limits<double>::quiet_NaN()) == "NaN");

  BOOST_TEST(DUT::Write(
                 std::numeric_limits<double>::infinity(),
                 DUT::Options().set_standard(true)) == "null");
  BOOST_TEST(DUT::Write(
                 -std::numeric_limits<double>::infinity(),
                 DUT::Options().set_standard(true)) == "null");
  BOOST_TEST(DUT::Write(
                 std::numeric_limits<double>::quiet_NaN(),
                 DUT::Options().set_standard(true)) == "null");
}
