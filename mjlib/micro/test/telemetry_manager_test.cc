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

#include "mjlib/micro/telemetry_manager.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/visitor.h"

#include "mjlib/micro/async_exclusive.h"
#include "mjlib/micro/command_manager.h"
#include "mjlib/micro/event_queue.h"
#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/required_success.h"
#include "mjlib/micro/stream_pipe.h"

#include "mjlib/micro/test/reader.h"

using namespace mjlib::micro;

namespace {
struct MyData {
  int32_t value = 0;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value));
  }
};

struct OtherData {
  int16_t stuff = 0;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(stuff));
  }
};

struct Fixture {
  SizedPool<> pool;
  EventQueue event_queue;
  StreamPipe pipe{event_queue.MakePoster()};
  test::Reader reader{pipe.side_b()};
  AsyncExclusive<AsyncWriteStream> write_stream{pipe.side_a()};
  CommandManager command_manager{&pool, pipe.side_a(), &write_stream};
  TelemetryManager dut{&pool, &command_manager, &write_stream};
  MyData my_data;
  OtherData other_data;

  VoidCallback my_data_update;
  VoidCallback other_data_update;

  Fixture() {
    my_data_update = dut.Register("my_data", &my_data);
    other_data_update = dut.Register("other_data", &other_data);
    command_manager.AsyncStart();
  }

  void ExpectResponse(const std::string_view& message) {
    BOOST_TEST(reader.data_.str() == message);
    reader.data_.str("");
  }

  void ExpectResponsePrefix(const std::string_view& message) {
    BOOST_TEST(reader.data_.str().substr(0, message.size()) == message);
    reader.data_.str("");
  }

  void Command(const std::string_view& message) {
    RequiredSuccess required_success;
    AsyncWrite(*pipe.side_b(), message, required_success.Make());
    event_queue.Poll();
  }
};

/// Turn a string literal into a std::string_view, including any
/// embedded null characters.
template <typename Array>
std::string_view str(const Array& array) {
  return std::string_view(array, sizeof(array) - 1);
}
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerGetUnknown, Fixture) {
  Command("tel get unknown\n");
  ExpectResponse("unknown name\r\n");
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerGet, Fixture) {
  Command("tel get my_data\n");
  ExpectResponse(str("emit my_data\r\n\x04\x00\x00\x00\x00\x00\x00\x00"));
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerList, Fixture) {
  Command("tel list\n");
  ExpectResponse("my_data\r\nother_data\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerSchema, Fixture) {
  Command("tel schema my_data\n");
  ExpectResponsePrefix("schema my_data\r\n");
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerRate, Fixture) {
  Command("tel rate my_data 20\n");
  ExpectResponse("OK\r\n");

  // This should result in the data being emitted twice.
  for (int i = 0; i < 50; i++) {
    dut.PollMillisecond();
    event_queue.Poll();
  }

  ExpectResponse(
      str("emit my_data\r\n\x04\x00\x00\x00\x00\x00\x00\x00"
          "emit my_data\r\n\x04\x00\x00\x00\x00\x00\x00\x00"));

  Command("tel stop\n");
  ExpectResponse("OK\r\n");

  for (int i = 0; i < 50; i++) {
    dut.PollMillisecond();
    event_queue.Poll();
  }

  ExpectResponse("");
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerFmt, Fixture) {
  Command("tel fmt my_data 1\n");
  ExpectResponse("OK\r\n");

  Command("tel get my_data\n");
  ExpectResponse("my_data.value 0\r\nOK\r\n");
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerText, Fixture) {
  Command("tel text\n");
  ExpectResponse("OK\r\n");

  Command("tel get my_data\n");
  ExpectResponse("my_data.value 0\r\nOK\r\n");
}
