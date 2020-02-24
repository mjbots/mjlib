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

#include "mjlib/io/exclusive_command.h"

#include <boost/asio/io_context.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/async_types.h"

namespace io = mjlib::io;
using io::ExclusiveCommand;

BOOST_AUTO_TEST_CASE(ExclusiveCommandTest) {
  boost::asio::io_context context;
  ExclusiveCommand dut{context.get_executor()};
  using Callback = io::VoidCallback;

  int item1_started = 0;
  int item1_done = 0;
  Callback item1_callback;
  dut.Invoke(
      [&](Callback done_callback) {
        item1_started++;
        item1_callback = std::move(done_callback);
      },
      [&]() {
        item1_done++;
      });

  int item2_started = 0;
  int item2_done = 0;
  Callback item2_callback;
  dut.Invoke(
      [&](Callback done_callback) {
        item2_started++;
        item2_callback = std::move(done_callback);
      },
      [&]() {
        item2_done++;
      });

  // Nothing should have been kicked off yet since we haven't polled.
  BOOST_TEST(item1_started == 0);
  BOOST_TEST(item1_done == 0);
  BOOST_TEST(item2_started == 0);
  BOOST_TEST(item2_done == 0);

  context.poll();
  context.reset();

  // Now the first item should have started.
  BOOST_TEST(item1_started == 1);
  BOOST_TEST(item1_done == 0);
  BOOST_TEST(item2_started == 0);
  BOOST_TEST(item2_done == 0);

  item1_callback();

  BOOST_TEST(item1_started == 1);
  BOOST_TEST(item1_done == 1);
  BOOST_TEST(item2_started == 0);
  BOOST_TEST(item2_done == 0);

  context.poll();
  context.reset();

  BOOST_TEST(item1_started == 1);
  BOOST_TEST(item1_done == 1);
  BOOST_TEST(item2_started == 1);
  BOOST_TEST(item2_done == 0);

  item2_callback();
  BOOST_TEST(item2_done == 1);
}

BOOST_AUTO_TEST_CASE(CancelTest) {
  boost::asio::io_context context;
  ExclusiveCommand dut{context.get_executor()};
  using Callback = io::VoidCallback;

  int item1_started = 0;
  int item1_done = 0;
  Callback item1_callback;
  const auto nonce1 = dut.Invoke(
      [&](Callback done_callback) {
        item1_started++;
        item1_callback = std::move(done_callback);
      },
      [&]() {
        item1_done++;
      });

  int item2_started = 0;
  int item2_done = 0;
  Callback item2_callback;
  const auto nonce2 = dut.Invoke(
      [&](Callback done_callback) {
        item2_started++;
        item2_callback = std::move(done_callback);
      },
      [&]() {
        item2_done++;
      });

  {
    const auto erased = dut.remove(nonce1);
    // We can't cancel this, because it gets invoked right away.
    BOOST_TEST(erased == 0);
  }
  {
    const auto erased = dut.remove(nonce2);
    // But we can remove this one.
    BOOST_TEST(erased == 1);
  }

  context.poll();
  context.reset();

  BOOST_TEST(item1_started == 1);
  BOOST_TEST(item1_done == 0);

  item1_callback();

  context.poll();
  context.reset();

  BOOST_TEST(item1_done == 1);
  BOOST_TEST(item2_started == 0);
}

BOOST_AUTO_TEST_CASE(ExclusiveCommandMoveOnly) {
  boost::asio::io_context context;
  ExclusiveCommand dut{context.get_executor()};
  using Callback = io::VoidCallback;

  int item1_started = 0;
  int item1_done = 0;
  Callback item1_callback;

  fu2::function<void (Callback)> cmd1 = [&](Callback done_callback) {
    item1_started++;
    item1_callback = std::move(done_callback);
  };

  fu2::function<void ()> hnd1 = [&]() {
    item1_done++;
  };

  dut.Invoke(std::move(cmd1), std::move(hnd1));
}
