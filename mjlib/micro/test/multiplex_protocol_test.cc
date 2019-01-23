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

#include "mjlib/micro/multiplex_protocol.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/stream_pipe.h"
#include "mjlib/micro/test/persistent_config_fixture.h"
#include "mjlib/micro/test/str.h"

namespace base = mjlib::base;
using namespace mjlib::micro;
using test::str;

namespace {
class Server : public MultiplexProtocolServer::Server {
 public:
  uint32_t Write(MultiplexProtocol::Register,
                 const MultiplexProtocol::Value&) override {
    return 0;
  }

  MultiplexProtocol::ReadResult Read(
      MultiplexProtocol::Register, size_t) const override {
    return static_cast<uint32_t>(0);
  }
};

struct Fixture : test::PersistentConfigFixture {
  StreamPipe dut_stream{event_queue.MakePoster()};

  Server server;
  MultiplexProtocolServer dut{&pool, dut_stream.side_b(), &server, []() {
      return MultiplexProtocolServer::Options();
    }()};

  AsyncStream* tunnel{dut.MakeTunnel(9)};

  Fixture() {
    dut.Start();
  }
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

const uint8_t kClientToServer2[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x02,  // destination id
  0x0b,  // payload size
    0x40,  // client->server data
      0x09,  // channel 9
      0x08,  // data len
      't', 'e', 's', 't', ' ', 'a', 'n', 'd',
  0xc7, 0xc0,  // CRC
  0x00,  // null terminator
};

const uint8_t kClientToServerMultiple[] = {
  0x54, 0xab,  // header
  0x02,  // source id
  0x01,  // destination id
  0x0b,  // payload size
    0x40,  // client->server data
      0x09,  // channel 9
      0x08,  // data len
      'f', 'i', 'r', 's', 't', ' ', 'f', 'm',
  0xc6, 0x17,  // CRC

  0x54, 0xab,  // header
  0x02,  // source id
  0x01,  // destination id
  0x09,  // payload size
    0x40,  // client->server data
      0x09,  // channel 9
      0x06,  // data len
      'm', 'o', 'r', 'e', 's', 't',
  0x09, 0x68,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(MultiplexProtocolServerTest, Fixture) {
  char read_buffer[100] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  tunnel->AsyncReadSome(read_buffer, [&](base::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  BOOST_TEST(read_count == 0);
  event_queue.Poll();
  BOOST_TEST(read_count == 0);

  {
    int write_count = 0;
    AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
               [&](base::error_code ec) {
                 BOOST_TEST(!ec);
                 write_count++;
               });
    event_queue.Poll();
    BOOST_TEST(write_count == 1);
    BOOST_TEST(read_count == 1);
    BOOST_TEST(read_size == 8);
    BOOST_TEST(std::string_view(read_buffer, 8) == "test and");
  }
}

BOOST_FIXTURE_TEST_CASE(ServerWrongId, Fixture) {
  char read_buffer[100] = {};
  int read_count = 0;
  tunnel->AsyncReadSome(read_buffer, [&](base::error_code, ssize_t) {
      read_count++;
    });

  event_queue.Poll();
  BOOST_TEST(read_count == 0);

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kClientToServer2),
             [&](base::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });
  event_queue.Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 0);
  BOOST_TEST(dut.stats()->wrong_id == 1);
}

BOOST_FIXTURE_TEST_CASE(ServerTestReadSecond, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](base::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });
  event_queue.Poll();
  BOOST_TEST(write_count == 1);

  char read_buffer[100] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  tunnel->AsyncReadSome(read_buffer, [&](base::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  event_queue.Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(read_size == 8);
  BOOST_TEST(std::string_view(read_buffer, 8) == "test and");
}

BOOST_FIXTURE_TEST_CASE(ServerTestFragment, Fixture) {
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerMultiple),
             [&](base::error_code ec) {
               BOOST_TEST(!ec);
             });

  char read_buffer[3] = {};
  auto read = [&](const std::string& expected) {
    int read_count = 0;
    ssize_t read_size = 0;
    tunnel->AsyncReadSome(read_buffer, [&](base::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });
    event_queue.Poll();
    BOOST_TEST(read_count == 1);
    BOOST_TEST(read_size == expected.size());
    BOOST_TEST(std::string_view(read_buffer, read_size) == expected);
  };
  read("fir");
  read("st ");
  read("fmm");
  read("ore");
  read("st");

  // Now kick off a read that should stall until more data comes in.
  int read_count = 0;
  ssize_t read_size = 0;
  tunnel->AsyncReadSome(read_buffer, [&](base::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  event_queue.Poll();
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](base::error_code ec) {
               BOOST_TEST(!ec);
             });
  event_queue.Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(read_size == 3);
  BOOST_TEST(std::string_view(read_buffer, 3) == "tes");
}

namespace {
const uint8_t kClientToServerEmpty[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x03,  // payload size
    0x40,  // client->server data
      0x09,  // channel 9
      0x00,  // data len
  0xcf, 0xb2,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(ServerSendTest, Fixture) {
  int write_count = 0;
  ssize_t write_size = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](base::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        write_count++;
        write_size = size;
      });

  event_queue.Poll();
  BOOST_TEST(write_count == 0);

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](base::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  event_queue.Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerEmpty),
             [](base::error_code ec) { BOOST_TEST(!ec); });

  event_queue.Poll();

  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x10,  // payload size
     0x41,  // server->client
      0x09,  // channel 9
      0x0d,  // 13 bytes of data
      's', 't', 'u', 'f', 'f', ' ', 't', 'o', ' ', 't', 'e', 's', 't',
    0x9d, 0xd2,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}
