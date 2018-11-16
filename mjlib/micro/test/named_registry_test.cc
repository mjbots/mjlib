// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/micro/named_registry.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::micro;

BOOST_AUTO_TEST_CASE(BasicNamedRegistry) {
  NamedRegistry<int, 16> dut;


  {
    int* const not_found = dut.FindOrCreate("stuff", kFindOnly);
    BOOST_TEST(not_found == nullptr);
  }

  static constexpr const char* kStuff = "stuff";
  {
    int* const created = dut.FindOrCreate(kStuff, kAllowCreate);
    BOOST_TEST(created != nullptr);
    // The things should be default constructed.
    BOOST_TEST(*created == 0);
    *created = 10;
  }

  {
    int* const found_again = dut.FindOrCreate(kStuff, kAllowCreate);
    BOOST_TEST(found_again != nullptr);
    BOOST_TEST(*found_again == 10);
  }

  {
    int* const find_only = dut.FindOrCreate(kStuff, kFindOnly);
    BOOST_TEST(find_only != nullptr);
    BOOST_TEST(*find_only == 10);
  }
}
