// Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/pool_map.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::micro;

BOOST_AUTO_TEST_CASE(BasicPoolMap) {
  SizedPool pool;
  PoolMap<int, double> dut(&pool, 16);

  BOOST_TEST(dut.size() == 0);
  BOOST_TEST(dut.contains(1) == false);

  {
    const auto result = dut.insert({3, 6.0});
    BOOST_TEST(dut.size() == 1);
    BOOST_TEST(result.second == true);
    BOOST_TEST(result.first->first == 3);
    BOOST_TEST(result.first->second == 6.0);
    BOOST_TEST(result.first != dut.end());
    BOOST_TEST(dut[0].first == 3);
    BOOST_TEST(dut[0].second == 6.0);
  }

  {
    const auto result = dut.insert({3, 6.0});
    BOOST_TEST(dut.size() == 1);
    BOOST_TEST(result.second == false);
    BOOST_TEST(result.first->first == 3);
  }

  {
    const auto it = dut.find(3);
    BOOST_TEST(it != dut.end());
    BOOST_TEST(it->first == 3);
    BOOST_TEST(it->second == 6.0);
  }

  {
    const auto it = dut.find(1);
    BOOST_TEST(it == dut.end());
  }

  {
    const auto result = dut.insert({10, 1.0});
    BOOST_TEST(dut.size() == 2);
    BOOST_TEST(result.second == true);

    BOOST_TEST(dut.contains(3) == true);
    BOOST_TEST(dut.contains(10) == true);
    BOOST_TEST(dut.contains(11) == false);
  }
}
