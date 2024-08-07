// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/micro/pool_ptr.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::micro;

namespace {
struct HasConstructor {
  HasConstructor(int item1, int item2)
      : a(item1 + item2),
        b(item1 - item2) {}

  int a = 0;
  int b = 1;
};
}

BOOST_AUTO_TEST_CASE(BasicPoolPtr) {
  SizedPool<> pool;

  PoolPtr<int> int_ptr(&pool);

  BOOST_TEST(*int_ptr == 0);
  *int_ptr = 100;
  BOOST_TEST(*int_ptr == 100);

  PoolPtr<HasConstructor> hc_ptr(&pool, 10, 20);
  BOOST_TEST(hc_ptr->a == 30);
  BOOST_TEST(hc_ptr->b == -10);
}
