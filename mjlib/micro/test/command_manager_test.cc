// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/command_manager.h"

#include <boost/test/auto_unit_test.hpp>

#include <fmt/format.h>

#include "mjlib/base/string_span.h"

#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/stream_pipe.h"

#include "mjlib/micro/test/reader.h"

using namespace mjlib::micro;
namespace base = mjlib::base;

using test::Reader;

BOOST_AUTO_TEST_CASE(BasicCommandManager) {
  SizedPool<> pool;
  std::deque<VoidCallback> events;
  auto poster = [&](VoidCallback cbk) { events.push_back(cbk); };
  auto poll = [&]() {
    while (!events.empty()) {
      auto copy = events;
      events.clear();
      for (auto& item : copy) { item(); }
    }
  };

  StreamPipe pipe{poster};
  Reader reader{pipe.side_b()};

  AsyncExclusive<AsyncWriteStream> writer(pipe.side_a());

  CommandManager dut(&pool, pipe.side_a(), &writer);

  int cmd1_count = 0;
  char cmd1_response[20] = {};

  dut.Register(
      "cmd1",
      [&](const std::string_view& msg,
          const CommandManager::Response& response) {
        cmd1_count++;
        std::strcpy(cmd1_response,
                    fmt::format("size: {}\n", msg.size()).c_str());
        AsyncWrite(*response.stream, cmd1_response, response.callback);
    });

  dut.AsyncStart();

  poll();

  BOOST_TEST(cmd1_count == 0);

  int write_done = 0;

  // Send a command.
  AsyncWrite(*pipe.side_b(), std::string_view("cmd1 and more stuff\n"),
             [&](error_code ec) {
               BOOST_TEST(!ec);
               write_done++;
             });

  BOOST_TEST(write_done == 0);
  BOOST_TEST(cmd1_count == 0);
  BOOST_TEST(reader.data_.str().empty());

  poll();

  BOOST_TEST(write_done == 1);
  BOOST_TEST(cmd1_count == 1);
  BOOST_TEST(reader.data_.str() == "size: 14\n");
  reader.data_.str("");

  // Send more than one command in a single buffer.
  AsyncWrite(*pipe.side_b(), std::string_view("cmd1 things\ncmd1 a\n"),
             [&](error_code ec) {
               BOOST_TEST(!ec);
               write_done++;
             });

  poll();

  BOOST_TEST(write_done == 2);
  BOOST_TEST(cmd1_count == 3);
  BOOST_TEST(reader.data_.str() == "size: 6\nsize: 1\n");
}
