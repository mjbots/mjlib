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

#pragma once

#include "mjlib/micro/flash.h"

#include "mjlib/micro/persistent_config.h"
#include "mjlib/micro/test/command_manager_fixture.h"

namespace mjlib {
namespace micro {
namespace test {

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

struct PersistentConfigFixture : CommandManagerFixture {
  StubFlash flash;
  PersistentConfig persistent_config{pool, command_manager, flash};
};

}
}
}
