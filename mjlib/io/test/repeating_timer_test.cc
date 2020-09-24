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

#include "mjlib/io/repeating_timer.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/debug_deadline_service.h"

using namespace mjlib::io;

BOOST_AUTO_TEST_CASE(BasicRepeatingTimer) {
  boost::asio::io_context context;
  auto poll = [&]() {
    context.poll();
    context.reset();
  };
  auto* const debug_time = DebugDeadlineService::Install(context);
  auto now = boost::posix_time::ptime(
      boost::gregorian::date(2000, boost::gregorian::Jan, 1));
  debug_time->SetTime(now);

  RepeatingTimer dut(context.get_executor());
  BOOST_TEST(dut.cancel() == 0);

  const auto ms100 = boost::posix_time::milliseconds(100);
  const auto ms50 = boost::posix_time::milliseconds(50);

  int ms100_count = 0;
  mjlib::base::error_code ms100_last_error_ec;
  mjlib::base::error_code ms100_ec;

  auto ms100_callback = [&](const mjlib::base::error_code& ec) mutable {
    ms100_count++;
    if (ec) { ms100_last_error_ec = ec; }
    ms100_ec = ec;
  };

  dut.start(ms100, ms100_callback);

  BOOST_TEST(ms100_count == 0);

  poll();

  BOOST_TEST(ms100_count == 0);

  now += ms100 - boost::posix_time::milliseconds(1);
  debug_time->SetTime(now);

  poll();

  BOOST_TEST(ms100_count == 0);

  now += boost::posix_time::milliseconds(1);
  debug_time->SetTime(now);

  poll();

  BOOST_TEST(ms100_count == 1);
  BOOST_TEST(!ms100_ec);

  now += ms100 + ms50;
  debug_time->SetTime(now);
  poll();

  BOOST_TEST(ms100_count == 2);
  BOOST_TEST(!ms100_ec);

  // Verify that if we use up the remainder, we still get another
  // pulse.  i.e. the time starts from the regular period, not from
  // the last callback.
  now += ms50;
  debug_time->SetTime(now);
  poll();

  BOOST_TEST(ms100_count == 3);
  BOOST_TEST(!ms100_ec);

  // And if skip forward a bunch, we get a reduced number of
  // callbacks, but one of them has an error.
  now += ms100 + ms100 + ms100 + ms100 + ms100;
  debug_time->SetTime(now);
  poll();

  BOOST_TEST(ms100_count == 5);
  BOOST_TEST(!ms100_ec);
  BOOST_TEST(ms100_last_error_ec == boost::asio::error::operation_aborted);

  // And things work as normal from there.
  now += ms100 + ms50;
  debug_time->SetTime(now);
  poll();

  BOOST_TEST(ms100_count == 6);
  BOOST_TEST(!ms100_ec);

  dut.cancel();
  dut.start(ms100, ms100_callback);
  poll();
}
