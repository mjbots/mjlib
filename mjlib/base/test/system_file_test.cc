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

#include "mjlib/base/system_file.h"

#include <utility>

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::SystemFile;

BOOST_AUTO_TEST_CASE(SystemFileDefaultDestruct) {
  // Previously, the destructor unconditionally called fclose(fd_),
  // and a default-constructed SystemFile has fd_ == nullptr. fclose
  // on NULL is undefined and segfaults on glibc. Reaching the end of
  // this test without crashing is the regression check.
  { SystemFile f; }
  BOOST_TEST(true);
}

BOOST_AUTO_TEST_CASE(SystemFileMoveSourceDestruct) {
  // The move ctor and move-assignment leave the source's fd_ null.
  // Destroying the moved-from instance previously called
  // fclose(nullptr) and crashed.
  FILE* file = ::fopen("/dev/null", "r");
  BOOST_REQUIRE(file != nullptr);
  {
    SystemFile a(file);
    SystemFile b(std::move(a));
    // ~a runs first as scope unwinds; with the bug it would crash.
  }
  BOOST_TEST(true);
}
