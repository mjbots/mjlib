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

#include "mjlib/base/time_conversions.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/test/auto_unit_test.hpp>

namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(ConvertSecondsToDuration) {
  BOOST_TEST(base::ConvertSecondsToDuration(1.0) == boost::posix_time::seconds(1));
  BOOST_TEST(base::ConvertSecondsToDuration(-1.0) == boost::posix_time::seconds(-1));
  BOOST_TEST(base::ConvertSecondsToDuration(0.0) == boost::posix_time::seconds(0));
  BOOST_TEST(base::ConvertSecondsToDuration(std::numeric_limits<double>::signaling_NaN()).is_not_a_date_time());
  BOOST_TEST(base::ConvertSecondsToDuration(-std::numeric_limits<double>::infinity()).is_neg_infinity());
  BOOST_TEST(base::ConvertSecondsToDuration(std::numeric_limits<double>::infinity()).is_pos_infinity());
}

BOOST_AUTO_TEST_CASE(ConvertMicrosecondsToDuration) {
  BOOST_TEST(base::ConvertMicrosecondsToDuration(1000) == boost::posix_time::milliseconds(1));
  BOOST_TEST(base::ConvertMicrosecondsToDuration(0) == boost::posix_time::seconds(0));
  BOOST_TEST(base::ConvertMicrosecondsToDuration(-2000000) == boost::posix_time::seconds(-2));
  BOOST_TEST(base::ConvertMicrosecondsToDuration(std::numeric_limits<int64_t>::min()).is_neg_infinity());
  BOOST_TEST(base::ConvertMicrosecondsToDuration(std::numeric_limits<int64_t>::max()).is_pos_infinity());
  BOOST_TEST(base::ConvertMicrosecondsToDuration(std::numeric_limits<int64_t>::min() + 1).is_not_a_date_time());
}

BOOST_AUTO_TEST_CASE(ConvertDurationToSeconds) {
  BOOST_TEST(base::ConvertDurationToSeconds(boost::posix_time::milliseconds(1)) == 0.001);
  BOOST_TEST(base::ConvertDurationToSeconds(boost::posix_time::seconds(0)) == 0.0);
  BOOST_TEST(base::ConvertDurationToSeconds(boost::posix_time::seconds(-1)) == -1.0);
  BOOST_TEST(base::ConvertDurationToSeconds(boost::posix_time::neg_infin) ==
             -std::numeric_limits<double>::infinity());
  BOOST_TEST(base::ConvertDurationToSeconds(boost::posix_time::pos_infin) ==
             std::numeric_limits<double>::infinity());
  BOOST_TEST(!std::isfinite(base::ConvertDurationToSeconds(boost::posix_time::not_a_date_time)));
}

BOOST_AUTO_TEST_CASE(ConvertDurationToMicroseconds) {
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::milliseconds(1)) == 1000);
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::seconds(0)) == 0);
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::seconds(-1)) == -1000000);
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::neg_infin) ==
             -std::numeric_limits<int64_t>::min());
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::pos_infin) ==
             std::numeric_limits<int64_t>::max());
  BOOST_TEST(base::ConvertDurationToMicroseconds(boost::posix_time::not_a_date_time) ==
             (std::numeric_limits<int64_t>::min() + 1));
}

BOOST_AUTO_TEST_CASE(ConvertEpochSecondsToPtime) {
}

BOOST_AUTO_TEST_CASE(ConvertEpochMicrosecondsToPtime) {
}

BOOST_AUTO_TEST_CASE(ConvertPtimeToEpochSeconds) {
}

BOOST_AUTO_TEST_CASE(ConvertPtimeToEpochMicroseconds) {
}
