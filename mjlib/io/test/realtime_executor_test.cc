// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/io/realtime_executor.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/test/auto_unit_test.hpp>

using mjlib::io::RealtimeExecutor;

BOOST_AUTO_TEST_CASE(RealtimeExecutorTest) {
  boost::asio::io_context context;
  RealtimeExecutor dut{context.get_executor()};
  boost::asio::any_io_executor executor{dut};

  BOOST_TEST((
      boost::asio::query(
          boost::asio::require(executor, boost::asio::execution::blocking.never),
          boost::asio::execution::blocking) == boost::asio::execution::blocking.never));

  bool cb1_called{};
  bool cb2_called{};
  const auto cb2 = [&]() {
    BOOST_TEST(cb1_called);
    BOOST_TEST(!cb2_called);
    cb2_called = true;
  };
  const auto cb1 = [&]() {
    BOOST_TEST(!cb1_called);
    BOOST_TEST(!cb2_called);
    cb1_called = true;
    boost::asio::post(executor, cb2);
    BOOST_TEST(!cb2_called);
  };
  boost::asio::post(executor, cb1);
  BOOST_TEST(!cb1_called);
  BOOST_TEST(!cb2_called);
  context.poll();
  context.restart();
  BOOST_TEST(cb1_called);
  BOOST_TEST(cb2_called);
}
