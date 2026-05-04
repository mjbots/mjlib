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

// Regression check for the missing `inline` on
// detail::AsyncIgnoreUntilCheck in async_read.h. Including the
// header from a second TU in this test target (alongside
// async_read_test.cc) used to produce a multiple-definition link
// error; the build itself is the test.

#include "mjlib/micro/async_read.h"

#include <boost/test/auto_unit_test.hpp>

BOOST_AUTO_TEST_CASE(AsyncReadHeaderIncludesInTwoTus) {
  BOOST_TEST(true);
}
