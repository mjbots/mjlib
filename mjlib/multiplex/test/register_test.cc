// Copyright 2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/multiplex/register.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::multiplex::RegisterRequest;
using Value = mjlib::multiplex::Format::Value ;

BOOST_AUTO_TEST_CASE(BasicRegisterTest) {
  {
    RegisterRequest dut;
    BOOST_TEST(dut.buffer().empty());
    BOOST_TEST(dut.request_reply() == false);
  }

  {
    RegisterRequest dut;
    dut.ReadSingle(0x001, 0);
    BOOST_TEST(dut.buffer().size() == 2);
    BOOST_TEST(dut.buffer() == std::string_view("\x18\x01"));
    BOOST_TEST(dut.request_reply() == true);
  }

  {
    RegisterRequest dut;
    dut.ReadMultiple(0x002, 3, 1);
    BOOST_TEST(dut.buffer() == std::string_view("\x1d\x02\x03"));
    BOOST_TEST(dut.request_reply() == true);
  }

  {
    RegisterRequest dut;
    dut.WriteSingle(0x002, Value(static_cast<int32_t>(0x22)));
    BOOST_TEST(dut.buffer() == std::string_view("\x12\x02\x22\x00\x00\x00", 6));
  }

  {
    RegisterRequest dut;
    dut.WriteMultiple(0x03, { Value(0.0f), Value(1.0f), });
    BOOST_TEST(dut.buffer().size() == 11);
  }
}
