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

#include "mjlib/base/error_code.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::error_code;

BOOST_AUTO_TEST_CASE(DefaultErrorCodeTest) {
  error_code dut;
  BOOST_TEST(!dut);
  BOOST_TEST(dut.message() == "");
  BOOST_TEST(!dut.boost_error_code());
  BOOST_TEST(dut == dut);
  BOOST_TEST(!(dut != dut));

  dut.Append("message");
  BOOST_TEST(dut.message() == "message");
}

BOOST_AUTO_TEST_CASE(Append) {
  auto dut = error_code::einval("");
  dut.Append("stuff");
  BOOST_TEST(dut.message() == "generic:22 Invalid argument\nstuff");
}

BOOST_AUTO_TEST_CASE(ErrorCodeTest) {
  auto dut = error_code::einval("failure");
  BOOST_TEST(!!dut);
  BOOST_TEST(dut.message() == "generic:22 Invalid argument\nfailure");
  BOOST_TEST(!!dut.boost_error_code());

  dut.Append("message");
  BOOST_TEST(dut.message() == "generic:22 Invalid argument\nfailure\nmessage");
}
