// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/io/async_sequence.h"

#include <boost/asio/io_context.hpp>
#include <boost/test/auto_unit_test.hpp>

using mjlib::io::AsyncSequence;

namespace {
struct Fixture {
  void Poll() {
    context.poll();
    context.reset();
  }

  void PollOne() {
    context.poll_one();
    context.reset();
  }

  boost::asio::io_context context;
  boost::asio::any_io_executor executor{context.get_executor()};
};
}

BOOST_FIXTURE_TEST_CASE(AsyncSequenceEmpty, Fixture) {
  AsyncSequence dut(executor);
}

BOOST_FIXTURE_TEST_CASE(AsyncSequenceBasic, Fixture) {
  AsyncSequence dut(executor);

  int handler1 = 0;
  int handler2 = 0;
  int done = 0;

  dut.Add([&](auto handler) {
         handler1++;
         handler(mjlib::base::error_code());
       })
     .Add([&](auto handler) {
         handler2++;
         handler(mjlib::base::error_code());
       })
     .Start([&](auto ec) {
         BOOST_TEST(!ec);
         done++;
       });

  PollOne();
  BOOST_TEST(handler1 == 1);
  BOOST_TEST(handler2 == 0);
  BOOST_TEST(done == 0);

  PollOne();
  BOOST_TEST(handler1 == 1);
  BOOST_TEST(handler2 == 1);
  BOOST_TEST(done == 0);

  PollOne();
  BOOST_TEST(handler1 == 1);
  BOOST_TEST(handler2 == 1);
  BOOST_TEST(done == 1);
}

BOOST_FIXTURE_TEST_CASE(AsyncSequenceError, Fixture) {
  AsyncSequence dut(executor);

  int handler1 = 0;
  int handler2 = 0;
  int done = 0;

  dut.Add([&](auto handler) {
         handler1++;
         handler(boost::asio::error::operation_aborted);
       }, "my operation")
     .Add([&](auto handler) {
         handler2++;
         handler(mjlib::base::error_code());
       })
     .Start([&](auto ec) {
         BOOST_TEST(ec == boost::asio::error::operation_aborted);
         BOOST_TEST(ec.message().find("my operation") != std::string::npos);
         done++;
       });

  PollOne();
  BOOST_TEST(handler1 == 1);
  BOOST_TEST(handler2 == 0);
  BOOST_TEST(done == 0);

  PollOne();
  BOOST_TEST(handler1 == 1);
  BOOST_TEST(handler2 == 0);
  BOOST_TEST(done == 1);
}
