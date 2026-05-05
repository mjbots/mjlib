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

#include "mjlib/io/debug_deadline_service.h"

#include <boost/asio/error.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/deadline_timer.h"

namespace io = mjlib::io;

BOOST_AUTO_TEST_CASE(DebugDeadlineServiceDestroyWithPendingWait) {
  // boost::asio::basic_deadline_timer documents that destruction
  // cancels any outstanding async_wait. Previously
  // DebugDeadlineService::Timer's destructor was empty, so the
  // queue entry persisted with a dangling Timer*; SetTime past the
  // deadline then walked the freed object and posted the user's
  // handler with a *success* error code instead of operation_aborted.
  boost::asio::io_context context;
  auto poll = [&] {
    context.poll();
    context.restart();
  };
  auto* const debug_time = io::DebugDeadlineService::Install(context);

  auto t0 = boost::posix_time::ptime(
      boost::gregorian::date(2000, boost::gregorian::Jan, 1));
  debug_time->SetTime(t0);

  int callback_count = 0;
  mjlib::base::error_code last_ec;

  {
    io::DeadlineTimer timer(context.get_executor());
    timer.expires_at(t0 + boost::posix_time::milliseconds(100));
    timer.async_wait(
        [&](const mjlib::base::error_code& ec) {
          ++callback_count;
          last_ec = ec;
        });
    // Timer goes out of scope here with a pending wait. The
    // destructor must cancel; the handler should run on the next
    // poll() with operation_aborted.
  }

  poll();
  BOOST_TEST(callback_count == 1);
  BOOST_TEST(last_ec == boost::asio::error::operation_aborted);

  // Advancing past the original deadline must not trigger any
  // additional callback (and pre-fix would dereference the freed
  // Timer here).
  debug_time->SetTime(t0 + boost::posix_time::milliseconds(200));
  poll();
  BOOST_TEST(callback_count == 1);
}
