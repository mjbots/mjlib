// Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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

#include <boost/algorithm/string.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/telemetry/format.h"

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
  ExpectResponse("ERR unknown name\r\n");
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

namespace {
std::map<std::string, int> CountReceipts(std::string data) {
  std::map<std::string, int> result;

  while (!data.empty()) {
    const size_t pos = data.find("\r\n");
    if (pos == std::string::npos) { break; }
    BOOST_TEST_REQUIRE(boost::starts_with(data, "emit "));

    const std::string name = data.substr(5, pos - 5);
    data = data.substr(pos + 2);
    mjlib::base::BufferReadStream buffer_stream(data);
    mjlib::telemetry::ReadStream stream{buffer_stream};
    const auto maybe_size = stream.Read<uint32_t>();
    if (!maybe_size) { break; }

    const auto size = *maybe_size;
    if (data.size() < (size + 4)) {
      break;
    }
    data = data.substr(size + 4);

    result[name]++;
  }

  return result;
}
}

BOOST_FIXTURE_TEST_CASE(TelemetryManagerOverloadTest, Fixture) {
  // When reads are serviced intermittently, we want to make sure that
  // all channels get equal chance to have their data emitted.
  Command("tel rate my_data 20\n");
  ExpectResponse("OK\r\n");
  Command("tel rate other_data 20\n");
  ExpectResponse("OK\r\n");

  // With no limiting of reads, we should get full data from each.
  for (int i = 0; i < 105; i++) {
    dut.PollMillisecond();
    event_queue.Poll();
  }

  {
    auto counts = CountReceipts(reader.data_.str());
    reader.data_.str("");

    BOOST_TEST(counts["my_data"] == 5);
    BOOST_TEST(counts["other_data"] == 5);
  }

  // Now we will limit our reads to only once per 20ms.
  reader.AllowReads(0);
  for (int i = 0; i < 200; i++) {
    if ((i % 20) == 0) { reader.AllowReads(2); }
    dut.PollMillisecond();
    event_queue.Poll();
  }

  {
    // We should still have an equal distribution of counts.
    auto counts = CountReceipts(reader.data_.str());
    reader.data_.str("");

    BOOST_TEST(counts["my_data"] == 3);
    BOOST_TEST(counts["other_data"] == 3);
  }
}
