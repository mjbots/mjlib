// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/micro/test/command_manager_fixture.h"

using namespace mjlib::micro;

using test::str;

namespace {
class StubFlash : public FlashInterface {
 public:
  Info GetInfo() override {
    return {buffer_, buffer_ + sizeof(buffer_)};
  }

  void Erase() override {
    std::memset(buffer_, 0, sizeof(buffer_));
  }

  void Unlock() override {
    BOOST_TEST(locked_);
    locked_ = false;
  }

  void Lock() override {
    BOOST_TEST(!locked_);
    locked_ = true;
  }

  void ProgramByte(char* ptr, uint8_t value) override {
    BOOST_TEST(!locked_);
    *ptr = value;
  }

  char buffer_[4096] = {};
  bool locked_ = true;
};

struct Fixture : test::CommandManagerFixture {
  StubFlash flash;
  PersistentConfig dut{pool, command_manager, flash};

  int my_data_count = 0;
  int other_data_count = 0;

  Fixture() {
    dut.Register("my_data", &my_data, [this]() {
        this->my_data_count++;
      });
    dut.Register("other_data", &other_data, [this]() {
        this->other_data_count++;
      });
  }
};
}

BOOST_FIXTURE_TEST_CASE(PersistentConfigEnumerate, Fixture) {
  Command("conf enumerate\n");
  ExpectResponse("my_data.value 0\r\nother_data.stuff 0\r\nOK\r\n");
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

  Command("conf load\n");
  ExpectResponse("OK\r\n");

  BOOST_TEST(my_data.value == 76);
  BOOST_TEST(other_data.stuff == 23);
}
