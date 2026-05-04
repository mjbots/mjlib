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

#include "mjlib/micro/callback_table.h"

#include <utility>

#include <boost/test/auto_unit_test.hpp>

namespace micro = mjlib::micro;

BOOST_AUTO_TEST_CASE(CallbackTableTest) {
  int count = 0;
  auto wrapped = micro::CallbackTable::MakeFunction([&]() { count++; });

  BOOST_TEST(count == 0);
  (*wrapped.raw_function)();
  BOOST_TEST(count == 1);
}

BOOST_AUTO_TEST_CASE(CallbackTableMoveAssignReleasesPriorSlot) {
  // Previously, Callback::operator=(Callback&&) overwrote raw_function
  // without freeing the slot the destination already held. Each such
  // overwrite permanently consumed one of the 10 fixed slots, so
  // after at most 10 overwrites MakeFunction would MJ_ASSERT(false)
  // even with no callbacks actually live.
  int counter_a = 0;
  int counter_b = 0;
  auto a = micro::CallbackTable::MakeFunction([&] { counter_a++; });
  auto b = micro::CallbackTable::MakeFunction([&] { counter_b++; });
  const auto a_handle = a.raw_function;
  BOOST_REQUIRE(a_handle != nullptr);
  BOOST_REQUIRE(b.raw_function != nullptr);
  BOOST_REQUIRE(a_handle != b.raw_function);

  a = std::move(b);

  // A fresh MakeFunction should reuse the slot that `a` previously
  // held, i.e. hand back the same handle. Pre-fix the slot was
  // permanently leaked and MakeFunction would return some later
  // handle instead.
  int counter_c = 0;
  auto c = micro::CallbackTable::MakeFunction([&] { counter_c++; });
  BOOST_TEST(c.raw_function == a_handle);
}
