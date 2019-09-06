// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/windowed_average.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::base;

BOOST_AUTO_TEST_CASE(WindowedAverageTest) {
  WindowedAverage<int16_t, 4, int32_t> dut;
  BOOST_TEST(dut.average() == 0);

  dut.Add(2);
  BOOST_TEST(dut.average() == 2);

  dut.Add(4);
  BOOST_TEST(dut.average() == 3);

  dut.Add(6);
  BOOST_TEST(dut.average() == 4);

  dut.Add(8);
  BOOST_TEST(dut.average() == 5);

  // Finally, drop the first element.
  dut.Add(10);
  BOOST_TEST(dut.average() == 7);
}
