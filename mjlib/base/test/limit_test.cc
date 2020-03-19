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

#include "mjlib/base/limit.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::Limit;

BOOST_AUTO_TEST_CASE(LimitTest) {
  BOOST_TEST(Limit(0, -1, 1) == 0);
  BOOST_TEST(Limit(0.0, -1.0, 1.0) == 0.0);
  BOOST_TEST(Limit(-2.0, -1.0, 1.0) == -1.0);
  BOOST_TEST(Limit(2.0, -1.0, 1.0) == 1.0);
  BOOST_TEST(
      Limit(2.0, std::numeric_limits<double>::signaling_NaN(), 1.0) == 1.0);
  BOOST_TEST(
      Limit(-2.0, std::numeric_limits<double>::signaling_NaN(), 1.0) == -2.0);
  BOOST_TEST(
      Limit(2.0, -1.0, std::numeric_limits<double>::signaling_NaN()) == 2.0);
  BOOST_TEST(
      Limit(-2.0, -1.0, std::numeric_limits<double>::signaling_NaN()) == -1.0);
}
