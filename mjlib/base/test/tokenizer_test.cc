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

#include "mjlib/base/tokenizer.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::base;

BOOST_AUTO_TEST_CASE(BasicTokenizer) {
  static constexpr const char* kToTokenize = "stuff that I want to send";
  static constexpr const char* kDelimeters = " ";

  Tokenizer dut(kToTokenize, kDelimeters);
  BOOST_TEST(dut.remaining() == std::string_view(kToTokenize));

  {
    const auto first = dut.next();
    BOOST_TEST(first == std::string_view("stuff"));
    BOOST_TEST(dut.remaining() == std::string_view("that I want to send"));
  }

  {
    const auto next = dut.next();
    BOOST_TEST(next == std::string_view("that"));
    BOOST_TEST(dut.remaining() == std::string_view("I want to send"));
  }

  dut.next();
  dut.next();
  dut.next();

  {
    const auto last = dut.next();
    BOOST_TEST(last == std::string_view("send"));
    BOOST_TEST(dut.remaining() == std::string_view(""));
  }

  {
    const auto past_end = dut.next();
    BOOST_TEST(past_end == std::string_view());
  }
}
