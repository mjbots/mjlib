// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/test/command_manager_fixture.h"
#include "mjlib/micro/test/str.h"

using namespace mjlib::micro;

using test::str;

namespace {
struct Fixture : test::CommandManagerFixture {
  TelemetryManager dut{&pool, &command_manager, &write_stream};

  Fixture() {
    my_data_update = dut.Register("my_data", &my_data);
    other_data_update = dut.Register("other_data", &other_data);
  }
};
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
