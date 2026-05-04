// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/base/system_error.h"

#include <string>

#include <boost/test/auto_unit_test.hpp>

namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(SystemErrorWhatReflectsCodeMutation) {
  // The non-const code() accessor exists so callers can layer
  // additional context onto a propagating error via
  // error_code::Append. Previously what() cached the formatted
  // message on the first call and never invalidated it, so any
  // Append performed after a what() call disappeared from
  // subsequent what() output.
  base::system_error e = base::system_error::einval("first");
  const std::string before = e.what();
  BOOST_TEST(before.find("first") != std::string::npos);
  BOOST_TEST(before.find("second") == std::string::npos);

  e.code().Append("second");

  const std::string after = e.what();
  BOOST_TEST(after.find("first") != std::string::npos);
  BOOST_TEST(after.find("second") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(SystemErrorWhatPointerValidUntilNextCall) {
  // Sanity check that the c_str() returned from what() remains
  // readable through to the next mutation -- the cache buffer is
  // owned by the system_error instance.
  base::system_error e = base::system_error::einval("hello");
  const char* ptr = e.what();
  BOOST_TEST(std::string(ptr).find("hello") != std::string::npos);
}
