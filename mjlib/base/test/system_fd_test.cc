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

#include "mjlib/base/system_fd.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <utility>

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::SystemFd;

BOOST_AUTO_TEST_CASE(SystemFdMoveAssignmentClosesPriorFd) {
  int pipe_a[2] = {};
  int pipe_b[2] = {};
  BOOST_REQUIRE(::pipe(pipe_a) == 0);
  BOOST_REQUIRE(::pipe(pipe_b) == 0);

  SystemFd a{pipe_a[0]};
  SystemFd b{pipe_b[0]};
  const int original_a_fd = static_cast<int>(a);
  BOOST_REQUIRE(::fcntl(original_a_fd, F_GETFD) >= 0);

  a = std::move(b);

  // The fd that `a` previously owned must have been closed by the
  // move-assignment, otherwise it leaks.
  errno = 0;
  const int status = ::fcntl(original_a_fd, F_GETFD);
  BOOST_TEST(status == -1);
  BOOST_TEST(errno == EBADF);

  // Clean up the write ends, since SystemFd only owns the read ends.
  ::close(pipe_a[1]);
  ::close(pipe_b[1]);
}

BOOST_AUTO_TEST_CASE(SystemFdSelfMoveAssignmentDoesNotLose) {
  int pipe_fds[2] = {};
  BOOST_REQUIRE(::pipe(pipe_fds) == 0);

  SystemFd a{pipe_fds[0]};
  const int original_fd = static_cast<int>(a);

  // Deliberately exercise the self-move guard.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  a = std::move(a);
#pragma GCC diagnostic pop

  BOOST_TEST(static_cast<int>(a) == original_fd);
  BOOST_TEST(::fcntl(original_fd, F_GETFD) >= 0);

  ::close(pipe_fds[1]);
}
