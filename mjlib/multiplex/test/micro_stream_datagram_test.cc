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

#include "mjlib/multiplex/micro_stream_datagram.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/stream_pipe.h"
#include "mjlib/micro/test/persistent_config_fixture.h"
#include "mjlib/micro/test/str.h"

using namespace mjlib;
using multiplex::MicroStreamDatagram;
using mjlib::micro::test::str;

namespace {
struct Fixture : micro::test::PersistentConfigFixture {
  micro::StreamPipe dut_stream{event_queue.MakePoster()};

  MicroStreamDatagram dut{
    &pool, dut_stream.side_b(), MicroStreamDatagram::Options()};
};

// Now send in a valid frame that contains some data.
const uint8_t kClientToServer[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x0b,  // payload size
    0x40,  // client->server data
      0x09,  // channel 9
      0x08,  // data len
      't', 'e', 's', 't', ' ', 'a', 'n', 'd',
  0x62, 0x0f,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(MicroStreamDatagramTest, Fixture) {
  char read_buffer[100] = {};
  multiplex::MicroDatagramServer::Header read_header;

  int read_done = 0;
  micro::error_code read_ec;
  int read_size = 0;
  dut.AsyncRead(&read_header, read_buffer, [&](auto ec, auto bytes) {
      read_done++;
      read_ec = ec;
      read_size = bytes;
    });

  BOOST_TEST(read_done == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
             });

  BOOST_TEST(read_done == 0);
  event_queue.Poll();
  BOOST_TEST(read_done == 1);
  BOOST_TEST(read_size == 11);
  BOOST_TEST(!read_ec);
}
