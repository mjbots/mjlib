// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/persistent_config.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/test/persistent_config_fixture.h"
#include "mjlib/micro/test/str.h"

using namespace mjlib::micro;

using test::str;

namespace {
struct Fixture : test::PersistentConfigFixture {
  PersistentConfig& dut = persistent_config;

  int my_data_count = 0;
  int other_data_count = 0;

  test::MyData non_enumerated;
  int non_enumerated_count = 0;

  Fixture() {
    dut.Register("my_data", &my_data, [this]() {
        this->my_data_count++;
      });
    dut.Register("other_data", &other_data, [this]() {
        this->other_data_count++;
      });
    PersistentConfig::RegisterOptions register_options;
    register_options.enumerate = false;
    dut.Register("non_enumerated", &non_enumerated, [this]() {
        this->non_enumerated_count++;
      },
      register_options);
  }
};
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigEnumerate, Fixture) {
  Command("conf enumerate\n");
  ExpectResponse("my_data.value 0\r\nother_data.stuff 0\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigEnumerateGroup1, Fixture) {
  Command("conf enumerate my_data\n");
  ExpectResponse("my_data.value 0\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigEnumerateGroup2, Fixture) {
  Command("conf enumerate other_data\n");
  ExpectResponse("other_data.stuff 0\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigEnumerateGroupUnknown, Fixture) {
  Command("conf enumerate notfound\n");
  ExpectResponse("ERR unknown group\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigList, Fixture) {
  Command("conf list\n");
  ExpectResponse("my_data\r\nother_data\r\nnon_enumerated\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigGet, Fixture) {
  Command("conf get my_data.value\n");
  ExpectResponse("0\r\n");
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigSet, Fixture) {
  BOOST_TEST(my_data.value == 0);
  Command("conf set my_data.value 39\n");
  ExpectResponse("OK\r\n");
  BOOST_TEST(my_data.value == 39);

  Command("conf set non_enumerated.value 47\n");
  ExpectResponse("OK\r\n");
  BOOST_TEST(non_enumerated.value == 47);
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigFlash, Fixture) {
  my_data.value = 76;
  other_data.stuff = 23;
  Command("conf write\n");
  ExpectResponse("OK\r\n");

  BOOST_TEST(my_data.value == 76);
  BOOST_TEST(other_data.stuff == 23);

  Command("conf default\n");
  ExpectResponse("OK\r\n");

  BOOST_TEST(my_data.value == 0);
  BOOST_TEST(other_data.stuff == 0);

  BOOST_TEST(my_data_count == 0);
  Command("conf load\n");
  ExpectResponse("OK\r\n");

  BOOST_TEST(my_data_count == 1);
  BOOST_TEST(my_data.value == 76);
  BOOST_TEST(other_data.stuff == 23);
}
