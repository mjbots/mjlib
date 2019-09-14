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

#include "mjlib/base/json5_read_archive.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::Json5ReadArchive;
using DUT = Json5ReadArchive;

BOOST_AUTO_TEST_CASE(Json5ReadValidNumbers) {
  BOOST_TEST(DUT::Read<int>("2") == 2);
  BOOST_TEST(DUT::Read<uint64_t>("18446744073709551615") ==
             std::numeric_limits<uint64_t>::max());
  BOOST_TEST(DUT::Read<int64_t>("-9223372036854775808") ==
             std::numeric_limits<int64_t>::min());
  BOOST_TEST(DUT::Read<int64_t>("9223372036854775807") ==
             std::numeric_limits<int64_t>::max());

  BOOST_TEST(DUT::Read<double>("0") == 0.0);
  BOOST_TEST(DUT::Read<double>("0.0") == 0.0);
  BOOST_TEST(DUT::Read<double>("+0.0") == 0.0);
  BOOST_TEST(DUT::Read<double>("-0.0") == 0.0);

  BOOST_TEST(DUT::Read<double>("1") == 1.0);
  BOOST_TEST(DUT::Read<double>("356") == 356.0);
  BOOST_TEST(DUT::Read<double>("1.2") == 1.2);
  BOOST_TEST(DUT::Read<double>("+1.2") == 1.2);
  BOOST_TEST(DUT::Read<double>("-1.2") == -1.2);
  BOOST_TEST(DUT::Read<double>("1.2e3") == 1.2e3);
  BOOST_TEST(DUT::Read<double>("1.2e-3") == 1.2e-3);
  BOOST_TEST(DUT::Read<double>("1.2e-31") == 1.2e-31);
  BOOST_TEST(DUT::Read<double>("13.21e-31") == 13.21e-31);
  BOOST_TEST(DUT::Read<double>(".123") == 0.123);
  BOOST_TEST(DUT::Read<double>("Infinity") ==
             std::numeric_limits<double>::infinity());
  BOOST_TEST(DUT::Read<double>("-Infinity") ==
             -std::numeric_limits<double>::infinity());
  BOOST_TEST(!std::isfinite(DUT::Read<double>("NaN")));

  BOOST_TEST(DUT::Read<int>("0x10") == 16);
  BOOST_TEST(DUT::Read<int>("-0x10") == -16);
  BOOST_TEST(DUT::Read<int>("0o10") == 8);
  BOOST_TEST(DUT::Read<int>("-0o10") == -8);
  BOOST_TEST(DUT::Read<int>("0b10") == 2);
  BOOST_TEST(DUT::Read<int>("-0b10") == -2);
}

BOOST_AUTO_TEST_CASE(Json5ReadValidStrings) {
  BOOST_TEST(DUT::Read<std::string>("\"hello\"") == "hello");
  BOOST_TEST(DUT::Read<std::string>(
                 "\"\\\\\\b\\f\\n\\r\\t\\v\\x20\\'\\\"\"") ==
             "\\\b\f\n\r\t\v '\"");
}

BOOST_AUTO_TEST_CASE(Json5ReadTime) {
  BOOST_TEST(DUT::Read<boost::posix_time::ptime>(
                 "\"2002-01-20 23:59:59.000\"") ==
             boost::posix_time::time_from_string("2002-01-20 23:59:59.000"));
  BOOST_TEST(DUT::Read<boost::posix_time::time_duration>(
                 "\"23:59:59.000\"") ==
             boost::posix_time::duration_from_string("23:59:59.000"));
}

BOOST_AUTO_TEST_CASE(Json5ReadOptional) {
  BOOST_TEST((DUT::Read<std::optional<int>>("null") == std::optional<int>{}));
  BOOST_TEST((*DUT::Read<std::optional<int>>("1234") == 1234));
}
