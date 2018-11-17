// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/micro/stream_pipe.h"

#include <deque>

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::micro;
namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(BasicStreamPipe) {
  std::deque<VoidCallback> events;
  auto poster = [&](VoidCallback cbk) { events.push_back(cbk); };

  StreamPipe dut(poster);

  const char data_to_send[] = "stuff to send";
  char data_to_receive[4] = {};

  int write_complete = 0;
  ssize_t write_size = 0;

  dut.side_a()->AsyncWriteSome(
      data_to_send,
      [&](base::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        write_complete++;
        write_size = size;
      });

  BOOST_TEST(write_complete == 0);
  BOOST_TEST(events.empty());

  int read_complete = 0;
  ssize_t read_size = 0;

  dut.side_b()->AsyncReadSome(
      base::string_span(data_to_receive, 4),
      [&](base::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_complete++;
        read_size = size;
      });

  BOOST_TEST(write_complete == 0);
  BOOST_TEST(read_complete == 0);
  BOOST_TEST(!events.empty());

  while (!events.empty()) {
    events.front()();
    events.pop_front();
  }

  BOOST_TEST(write_complete == 1);
  BOOST_TEST(read_complete == 1);
  BOOST_TEST(std::string_view(data_to_receive, 4) == "stuf");
}
