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

#include "mjlib/micro/static_vector.h"

#include <boost/test/auto_unit_test.hpp>

namespace micro = mjlib::micro;

BOOST_AUTO_TEST_CASE(BasicStaticVector) {
  {
    const micro::StaticVector<int, 10> dut;
    BOOST_TEST(dut.empty());
    BOOST_TEST(dut.size() == 0);
    BOOST_TEST(dut.capacity() == 10);
    BOOST_TEST((dut.begin() == dut.end()));
  }
  {
    micro::StaticVector<int, 10> dut;
    BOOST_TEST(dut.empty());
    BOOST_TEST(dut.size() == 0);
    BOOST_TEST(dut.capacity() == 10);
    BOOST_TEST((dut.begin() == dut.end()));

    dut.push_back(13);
    BOOST_TEST(!dut.empty());
    BOOST_TEST(dut.size() == 1);
    BOOST_TEST((dut.begin() != dut.end()));
    BOOST_TEST(dut.front() == 13);
    BOOST_TEST(dut.back() == 13);
    BOOST_TEST(*dut.begin() == 13);
    BOOST_TEST(*(dut.end() - 1) == 13);
    BOOST_TEST(*dut.data() == 13);
    BOOST_TEST(dut[0] == 13);
    BOOST_TEST((dut.end() - dut.begin()) == 1);

    int count = 0;
    for (auto value : dut) {
      BOOST_TEST(value == 13);
      count++;
    }
    BOOST_TEST(count == 1);

    const auto copy = dut;

    dut.pop_back();
    BOOST_TEST(dut.empty());
    BOOST_TEST(dut.size() == 0);
    BOOST_TEST((dut.begin() == dut.end()));

    BOOST_TEST(!copy.empty());
    BOOST_TEST(copy.size() == 1);
    BOOST_TEST(copy[0] == 13);
  }
}
