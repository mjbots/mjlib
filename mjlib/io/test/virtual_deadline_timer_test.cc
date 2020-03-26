// Copyright 2016-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/io/deadline_timer.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib;

BOOST_AUTO_TEST_CASE(BasicVirtualDeadlineTimer) {
  boost::asio::io_context context;
  io::DeadlineTimer timer(context);
  timer.expires_from_now(boost::posix_time::milliseconds(1));
  timer.wait();
}
