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

#include <fcntl.h>

#include <cerrno>
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

BOOST_AUTO_TEST_CASE(SystemFileMoveAssignmentClosesPriorFile) {
  // Previously, operator=(SystemFile&&) overwrote fd_ without
  // calling fclose on the existing handle, leaking the underlying
  // FILE* and its kernel fd.
  FILE* file_a = ::fopen("/dev/null", "r");
  FILE* file_b = ::fopen("/dev/null", "r");
  BOOST_REQUIRE(file_a != nullptr);
  BOOST_REQUIRE(file_b != nullptr);
  const int original_a_fd = ::fileno(file_a);
  BOOST_REQUIRE(::fcntl(original_a_fd, F_GETFD) >= 0);

  SystemFile a(file_a);
  SystemFile b(file_b);
  a = std::move(b);

  errno = 0;
  const int status = ::fcntl(original_a_fd, F_GETFD);
  BOOST_TEST(status == -1);
  BOOST_TEST(errno == EBADF);
}

BOOST_AUTO_TEST_CASE(SystemFileSelfMoveAssignmentDoesNotLose) {
  FILE* file = ::fopen("/dev/null", "r");
  BOOST_REQUIRE(file != nullptr);
  const int original_fd = ::fileno(file);

  SystemFile a(file);

  // Deliberately exercise the self-move guard.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  a = std::move(a);
#pragma GCC diagnostic pop

  BOOST_TEST(static_cast<FILE*>(a) == file);
  BOOST_TEST(::fcntl(original_fd, F_GETFD) >= 0);
}
