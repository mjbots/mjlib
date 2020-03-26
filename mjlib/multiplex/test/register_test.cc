// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/fast_stream.h"

namespace base = mjlib::base;
using mjlib::multiplex::RegisterRequest;
using mjlib::multiplex::ParseRegisterReply;
using Value = mjlib::multiplex::Format::Value;
using ReadResult = mjlib::multiplex::Format::ReadResult;

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
    BOOST_TEST(dut.buffer() == std::string_view("\x11\x01", 2));
    BOOST_TEST(dut.request_reply() == true);
  }

  {
    RegisterRequest dut;
    dut.ReadMultiple(0x002, 3, 1);
    BOOST_TEST(dut.buffer() == std::string_view("\x17\x02"));
    BOOST_TEST(dut.request_reply() == true);
  }

  {
    RegisterRequest dut;
    dut.ReadMultiple(0x002, 5, 1);
    BOOST_TEST(dut.buffer() == std::string_view("\x14\x05\x02"));
    BOOST_TEST(dut.request_reply() == true);
  }

  {
    RegisterRequest dut;
    dut.WriteSingle(0x002, Value(static_cast<int32_t>(0x22)));
    BOOST_TEST(dut.buffer() == std::string_view("\x09\x02\x22\x00\x00\x00", 6));
  }

  {
    RegisterRequest dut;
    dut.WriteMultiple(0x03, { Value(0.0f), Value(0.0f), });
    BOOST_TEST(dut.buffer() ==
               std::string_view("\x0e\x03\x00\x00\x00\x00\x00\x00\x00\x00",
                                10));
  }

  {
    RegisterRequest dut;
    dut.WriteMultiple(0x03, { Value(0.0f), Value(0.0f),
            Value(0.0f), Value(0.0f)});
    BOOST_TEST(dut.buffer() ==
               std::string_view("\x0c\x04\x03"
                                "\x00\x00\x00\x00\x00\x00\x00\x00"
                                "\x00\x00\x00\x00\x00\x00\x00\x00",
                                19));
  }
}

BOOST_AUTO_TEST_CASE(ParseRegisterReplyTest) {
  {
    base::FastIStringStream data("\x21\x03\x01");
    const auto dut = ParseRegisterReply(data);
    BOOST_TEST(dut.size() == 1);
    BOOST_TEST((dut.at(0x03) == ReadResult(Value(static_cast<int8_t>(1)))));
  }
  {
    base::FastIStringStream data("\x26\x04\x06\x05\x04\x03");
    const auto dut = ParseRegisterReply(data);
    BOOST_TEST(dut.size() == 2);
    BOOST_TEST((dut.at(0x04) == ReadResult(Value(static_cast<int16_t>(0x0506)))));
    BOOST_TEST((dut.at(0x05) == ReadResult(Value(static_cast<int16_t>(0x0304)))));
  }
  {
    base::FastIStringStream data("\x30\x01\x05");
    const auto dut = ParseRegisterReply(data);
    BOOST_TEST(dut.size() == 1);
    BOOST_TEST((dut.at(0x01) == ReadResult(static_cast<uint32_t>(5))));
  }
  {
    base::FastIStringStream data("\x50");
    const auto dut = ParseRegisterReply(data);
    BOOST_TEST(dut.size() == 0);
  }
}
