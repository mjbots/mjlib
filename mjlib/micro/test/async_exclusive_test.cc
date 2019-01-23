// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/async_exclusive.h"

#include <optional>

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/assert.h"

using namespace mjlib::micro;

BOOST_AUTO_TEST_CASE(BasicAsyncExclusive) {
  int value = 0;
  AsyncExclusive<int> dut{&value};

  std::optional<VoidCallback> do_release1;

  dut.AsyncStart([&](int* resource, VoidCallback release) {
      MJ_ASSERT(resource == &value);
      do_release1 = release;
    });

  // This was the first thing, so it should be invoked right away.
  BOOST_TEST(!!do_release1);

  std::optional<VoidCallback> do_release2;
  dut.AsyncStart([&](int* resource, VoidCallback release) {
      MJ_ASSERT(resource == &value);
      do_release2 = release;
    });

  // This one should not be invoked until we finish our first
  // operation.
  BOOST_TEST(!do_release2);

  (*do_release1)();
  do_release1 = {};

  // And now our second callback has been invoked.
  BOOST_TEST(!!do_release2);

  // Just to make sure nothing crashes, release our resource a final
  // time.
  (*do_release2)();
  do_release2 = {};
}
