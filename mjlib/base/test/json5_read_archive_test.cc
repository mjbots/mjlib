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

BOOST_AUTO_TEST_CASE(Json5ReadValidNumbers) {
  BOOST_TEST(Json5ReadArchive::Read<int>("2") == 2);
  BOOST_TEST(Json5ReadArchive::Read<uint64_t>("18446744073709551615") ==
             std::numeric_limits<uint64_t>::max());
  BOOST_TEST(Json5ReadArchive::Read<int64_t>("-9223372036854775808") ==
             std::numeric_limits<int64_t>::min());
  BOOST_TEST(Json5ReadArchive::Read<int64_t>("9223372036854775807") ==
             std::numeric_limits<int64_t>::max());

  BOOST_TEST(Json5ReadArchive::Read<double>("0") == 0.0);
  BOOST_TEST(Json5ReadArchive::Read<double>("0.0") == 0.0);
  BOOST_TEST(Json5ReadArchive::Read<double>("+0.0") == 0.0);
  BOOST_TEST(Json5ReadArchive::Read<double>("-0.0") == 0.0);

  BOOST_TEST(Json5ReadArchive::Read<double>("1") == 1.0);
  BOOST_TEST(Json5ReadArchive::Read<double>("356") == 356.0);
  BOOST_TEST(Json5ReadArchive::Read<double>("1.2") == 1.2);
  BOOST_TEST(Json5ReadArchive::Read<double>("+1.2") == 1.2);
  BOOST_TEST(Json5ReadArchive::Read<double>("-1.2") == -1.2);
  BOOST_TEST(Json5ReadArchive::Read<double>("1.2e3") == 1.2e3);
  BOOST_TEST(Json5ReadArchive::Read<double>("1.2e-3") == 1.2e-3);
  BOOST_TEST(Json5ReadArchive::Read<double>("1.2e-31") == 1.2e-31);
  BOOST_TEST(Json5ReadArchive::Read<double>("13.21e-31") == 13.21e-31);
  BOOST_TEST(Json5ReadArchive::Read<double>(".123") == 0.123);
  BOOST_TEST(Json5ReadArchive::Read<double>("Infinity") ==
             std::numeric_limits<double>::infinity());
  BOOST_TEST(Json5ReadArchive::Read<double>("-Infinity") ==
             -std::numeric_limits<double>::infinity());
  BOOST_TEST(!std::isfinite(Json5ReadArchive::Read<double>("NaN")));
}
