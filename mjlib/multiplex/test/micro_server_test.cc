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

#include "mjlib/multiplex/micro_server.h"

#include <vector>

#include <boost/crc.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/stream_pipe.h"
#include "mjlib/micro/test/persistent_config_fixture.h"
#include "mjlib/micro/test/str.h"

#include "mjlib/multiplex/frame.h"
#include "mjlib/multiplex/micro_stream_datagram.h"

namespace base = mjlib::base;
using namespace mjlib::multiplex;
using mjlib::micro::test::str;
namespace micro = mjlib::micro;
namespace test = micro::test;

namespace {
class Server : public MicroServer::Server {
 public:
  void StartFrame() override {
    start_called_ = true;
  }

  WriteAction Write(MicroServer::Register reg,
                 const MicroServer::Value& value) override {
    BOOST_TEST(start_called_ == true);
    writes_.push_back({reg, value});
    return next_write_error_;
  }

  MicroServer::ReadResult Read(
      MicroServer::Register reg, size_t type_index) const override {
    BOOST_TEST(start_called_ == true);
    if (type_index == 0) {
      return MicroServer::Value(int8_values.at(reg));
    } else if (type_index == 2) {
      return MicroServer::Value(int32_values.at(reg));
    } else if (type_index == 3) {
      return MicroServer::Value(float_values.at(reg));
    }
    return static_cast<uint32_t>(1);
  }

  Action CompleteFrame() override {
    BOOST_TEST(start_called_ == true);
    start_called_ = false;
    return action_;
  }

  struct WriteValue {
    MicroServer::Register reg;
    MicroServer::Value value;
  };

  std::vector<WriteValue> writes_;
  WriteAction next_write_error_ = MicroServer::Server::kSuccess;

  std::map<uint32_t, int8_t> int8_values = {
    { 0, 3 },
  };

  std::map<uint32_t, int32_t> int32_values = {
    { 9, 0x09080706, },
    { 16, 0x19181716, },
  };
  std::map<uint32_t, float> float_values = {
    { 10, 1.0f },
    { 11, 2.0f },
  };

  bool start_called_ = false;
  Action action_ = kAccept;
};

struct Fixture : test::PersistentConfigFixture {
  micro::StreamPipe dut_stream{event_queue.MakePoster()};

  Server server;
  mjlib::multiplex::MicroStreamDatagram stream_datagram{
    &pool, dut_stream.side_b(), {}};
  MicroServer dut{&pool, &stream_datagram, []() {
      return MicroServer::Options();
    }()};

  micro::AsyncStream* tunnel{dut.MakeTunnel(9)};

  Fixture() {
    dut.Start(&server);
  }

  void Poll() {
    event_queue.Poll();
    dut.Poll();
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

BOOST_FIXTURE_TEST_CASE(MicroServerTest, Fixture) {
  auto run_test = [&]() {
    char read_buffer[100] = {};
    int read_count = 0;
    ssize_t read_size = 0;
    tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

    BOOST_TEST(read_count == 0);
    Poll();
    BOOST_TEST(read_count == 0);

    {
      int write_count = 0;
      AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
                 [&](micro::error_code ec) {
                   BOOST_TEST(!ec);
                   write_count++;
                 });
      Poll();
      BOOST_TEST(write_count == 1);
      BOOST_TEST(read_count == 1);
      BOOST_TEST(read_size == 8);
      BOOST_TEST(std::string_view(read_buffer, 8) == "test and");
    }
  };
  run_test();
  dut.config()->id = 0x81;  // The high bit should be ignored.
  run_test();
}

BOOST_FIXTURE_TEST_CASE(ServerWrongId, Fixture) {
  char read_buffer[100] = {};
  int read_count = 0;
  tunnel->AsyncReadSome(read_buffer, [&](micro::error_code, ssize_t) {
      read_count++;
    });

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kClientToServer2),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });
  Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 0);
  BOOST_TEST(dut.stats()->wrong_id == 1);
}

BOOST_FIXTURE_TEST_CASE(ServerTestReadSecond, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });
  Poll();
  BOOST_TEST(write_count == 1);

  char read_buffer[100] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(read_size == 8);
  BOOST_TEST(std::string_view(read_buffer, 8) == "test and");
}

BOOST_FIXTURE_TEST_CASE(ServerTestFragment, Fixture) {
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerMultiple),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
             });

  char read_buffer[3] = {};
  auto read = [&](const std::string& expected) {
    int read_count = 0;
    ssize_t read_size = 0;
    tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });
    Poll();
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
  tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  Poll();
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
             });
  Poll();
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
      [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        write_count++;
        write_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerEmpty),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

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

namespace {
const uint8_t kClientToServerPoll[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x03,  // payload size
    0x42,  // client->server data
      0x09,  // channel 9
      0x05,  // data len
  0x0a, 0x8c,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(ServerSendPollTest, Fixture) {
  int write_count = 0;
  ssize_t write_size = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        write_count++;
        write_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPoll),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x08,  // payload size
     0x41,  // server->client
      0x09,  // channel 9
      0x05,  // 13 bytes of data
      's', 't', 'u', 'f', 'f',
    0xc5, 0xa8,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

namespace {
const uint8_t kWriteSingle[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x03,  // payload size
    0x01,  // write single int8_t
      0x02,  // register 2
      0x20,  // value
  0xca, 0x60,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(WriteSingleTest, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kWriteSingle),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);

  BOOST_TEST(server.writes_.size() == 1);
  BOOST_TEST(server.writes_.at(0).reg == 2);
  BOOST_TEST(std::get<int8_t>(server.writes_.at(0).value) == 0x20);
}

namespace {
const uint8_t kWriteSingleBroadcast[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x7f,  // destination id
  0x03,  // payload size
    0x01,  // write single int8_t
      0x02,  // register 2
      0x20,  // value
  0xe4, 0xb2,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(WriteSingleBroadcast, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kWriteSingleBroadcast),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);

  BOOST_TEST(server.writes_.size() == 1);
  BOOST_TEST(server.writes_.at(0).reg == 2);
  BOOST_TEST(std::get<int8_t>(server.writes_.at(0).value) == 0x20);
}

namespace {
const uint8_t kWriteMultiple[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x08,  // payload size
    0x07,  // write int16_t * 3
      0x05,  // start register
      0x01, 0x03,  // value1
      0x03, 0x03,  // value1
      0x05, 0x03,  // value1
  0x87, 0xcc,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(WriteMultipleTest, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kWriteMultiple),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);

  BOOST_TEST(server.writes_.size() == 3);
  BOOST_TEST(server.writes_.at(0).reg == 5);
  BOOST_TEST(std::get<int16_t>(server.writes_.at(0).value) == 0x0301);

  BOOST_TEST(server.writes_.at(1).reg == 6);
  BOOST_TEST(std::get<int16_t>(server.writes_.at(1).value) == 0x0303);

  BOOST_TEST(server.writes_.at(2).reg == 7);
  BOOST_TEST(std::get<int16_t>(server.writes_.at(2).value) == 0x0305);
}

BOOST_FIXTURE_TEST_CASE(WriteErrorTest, Fixture) {
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  BOOST_TEST(read_count == 0);


  server.next_write_error_ = MicroServer::Server::kUnknownRegister;

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kWriteSingle),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x03,  // payload size
     0x30,  // write error
      0x02,  // register
      0x01,  // error
    0x0e, 0x52,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

namespace {
const uint8_t kReadSingle[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x04,  // payload size
    0x19,  // read single int32_t
      0x09,  // register
    0x19,  // read single int32_t
      0x10,  // register
  0xdb, 0x02,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(ReadSingleTest, Fixture) {
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kReadSingle),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x0c,  // payload size
     0x29,  // reply single int32_t
      0x09,  // register
      0x06, 0x07, 0x08, 0x09,  // value
     0x29,  // reply single int32_t
      0x10,  // register
      0x16, 0x17, 0x18, 0x19,  // value
    0x02, 0x72,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

namespace {
const uint8_t kReadMultiple[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x02,  // payload size
    0x1e,  // read multiple float x2
      0x0a,  // register
  0xa4, 0x07,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(ReadMultipleTest, Fixture) {
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kReadMultiple),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x0a,  // payload size
     0x2e,  // reply multiple float x2
      0x0a,  // register
      0x00, 0x00, 0x80, 0x3f,  // value
      0x00, 0x00, 0x00, 0x40,
    0xad, 0x46,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

BOOST_FIXTURE_TEST_CASE(ReadMultipleInt8s, Fixture) {
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  const uint8_t kReadMultiple[] = {
    0x54, 0xab,  // header
    0x82,  // source id
      0x01,  // destination id
      0x03,  // payload size
        0x10,  // read N int8s
        0x01,  // no regs
        0x00,   // start reg
    0xa8, 0x65,  // CRC
    0x00,  // null terminator
  };

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kReadMultiple),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x04,  // payload size
     0x20,  // reply single int x1
      0x01,
      0x00,
      0x03,
    0xc6, 0x52,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

namespace {
const uint8_t kNop[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x01,  // payload size
    0x50,  // nop
  0x1a, 0xd0,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(NopTest, Fixture) {
  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kNop),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  Poll();
  BOOST_TEST(write_count == 1);

  // Nothing bad should have happened.
  BOOST_TEST(dut.stats()->unknown_subframe == 0);
}

namespace {
const uint8_t kClientToServerPollWithWrite[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x06,  // payload size
    0x01,  // write 1 register
      0x00,  // register 0
      0x00,  // value 0
    0x42,  // client->server data
      0x09,  // channel 9
      0x05,  // data len
  0x6e, 0xf1,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(DiscardTest, Fixture) {
  int write_count = 0;
  ssize_t write_size = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        write_count++;
        write_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  server.next_write_error_ = MicroServer::Server::kDiscardRemaining;
  server.action_ = Server::kDiscard;

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollWithWrite),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);
  BOOST_TEST(dut.stats()->discards == 2);

  server.next_write_error_ = MicroServer::Server::kSuccess;
  server.action_ = Server::kAccept;

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollWithWrite),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);
  BOOST_TEST(dut.stats()->discards == 2);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x08,  // payload size
     0x41,  // server->client
      0x09,  // channel 9
      0x05,  // N bytes of data
      's', 't', 'u', 'f', 'f',
    0xc5, 0xa8,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

namespace {
const uint8_t kClientToServerPollFlow[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x04,  // payload size
    0x44,  // client poll server with ack
      0x09,  // channel 9
      0x00,  // packet_number 0
      0x05,  // max bytes
  0x19, 0xb5,  // CRC
  0x00,  // null terminator
};

const uint8_t kClientToServerPollFlowAck1[] = {
  0x54, 0xab,  // header
  0x82,  // source id
  0x01,  // destination id
  0x04,  // payload size
    0x44,  // client poll server with ack
      0x09,  // channel 9
      0x01,  // packet_number 1
      0x05,  // max bytes
  0x28, 0x86,  // CRC
  0x00,  // null terminator
};
}

BOOST_FIXTURE_TEST_CASE(FlowControlPollTest, Fixture) {
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  Poll();
  BOOST_TEST(write_count == 0);

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  // The write callback is NOT invoked yet; buffer is held until ack.
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x09,  // payload size
     0x43,  // server->client flow
      0x09,  // channel 9
      0x01,  // packet_number
      0x05,  // 5 bytes of data
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedResponse));
}

BOOST_FIXTURE_TEST_CASE(FlowControlRetransmitTest, Fixture) {
  // First, send data and get initial response.
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(read_count == 1);

  // Now send another poll with packet_number=0 (wrong ack, server sent 1).
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(read_count == 1);

  // Should get same response retransmitted.
  const uint8_t kExpectedRetransmit[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x09,  // payload size
     0x43,  // server->client flow
      0x09,  // channel 9
      0x01,  // packet_number (same as before)
      0x05,  // 5 bytes of data
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedRetransmit));
}

BOOST_FIXTURE_TEST_CASE(FlowControlAckTest, Fixture) {
  // The first AsyncWriteSome callback provides new data synchronously,
  // simulating an application that immediately queues more output.
  int write_count1 = 0;
  int write_count2 = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count1++;
        // Immediately provide new data.
        tunnel->AsyncWriteSome(
            "more!and extra",
            [&](micro::error_code ec2, ssize_t) {
              BOOST_TEST(!ec2);
              write_count2++;
            });
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Initial poll: data sent but callback not yet invoked.
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(write_count1 == 0);
  BOOST_TEST(read_count == 1);

  // Now ack with packet_number=1.  The first callback fires, which
  // synchronously provides new data, and the server sends it.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlowAck1),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(write_count1 == 1);
  BOOST_TEST(read_count == 1);
  // write_count2 is 0 because the new buffer is held until its ack.
  BOOST_TEST(write_count2 == 0);

  // Should get new data with incremented packet number.
  const uint8_t kExpectedNewData[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x09,  // payload size
     0x43,  // server->client flow
      0x09,  // channel 9
      0x02,  // packet_number (incremented)
      0x05,  // 5 bytes of data
      'm', 'o', 'r', 'e', '!',
    0x60, 0xa8,  // CRC
    0x00,  // null terminator
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedNewData));
}

// ---- Adversarial flow control tests ----

namespace {
// Truncated: only subframe type, no channel/packet_number/max_bytes
const uint8_t kFlowPollTruncJustType[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x01,        // payload size = 1
    0x44,
  0xaf, 0x82,  // CRC
  0x00,
};

// Truncated: subframe type + channel, no packet_number or max_bytes
const uint8_t kFlowPollTruncNoPacketNum[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x02,        // payload size = 2
    0x44,
    0x09,      // channel 9
  0xb3, 0xd6,  // CRC
  0x00,
};

// Truncated: type + channel + packet_number, no max_bytes
const uint8_t kFlowPollTruncNoMaxBytes[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x03,        // payload size = 3
    0x44,
    0x09,      // channel 9
    0x00,      // packet_number
  0x0f, 0x6e,  // CRC
  0x00,
};

// Flow poll on channel 7 (no tunnel allocated for it)
const uint8_t kFlowPollBadChannel[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x04,
    0x44,
    0x07,      // channel 7 (nonexistent)
    0x00,
    0x05,
  0x18, 0xae,  // CRC
  0x00,
};

// Flow poll with max_bytes = 0
const uint8_t kFlowPollZeroMax[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x04,
    0x44,
    0x09,      // channel 9
    0x00,      // packet_number 0
    0x00,      // max_bytes = 0
  0xbc, 0xe5,  // CRC
  0x00,
};

// Flow poll from source without response-request bit
const uint8_t kFlowPollNoResponse[] = {
  0x54, 0xab,
  0x02, 0x01,  // source = 0x02 (no response bit)
  0x04,
    0x44,
    0x09,
    0x00,
    0x05,
  0xe0, 0x1e,  // CRC
  0x00,
};

// Flow poll ack with packet_number=255 (wildly wrong)
const uint8_t kFlowPollAck255[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x04,
    0x44,
    0x09,
    0xff,      // packet_number = 255
    0x05,
  0xe6, 0xb6,  // CRC
  0x00,
};

// Flow poll ack=1 with max_bytes=0
const uint8_t kFlowPollAck1Max0[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x04,
    0x44,
    0x09,
    0x01,      // packet_number = 1
    0x00,      // max_bytes = 0
  0x8d, 0xd6,  // CRC
  0x00,
};

// Combined: write subframe + flow poll in same frame
const uint8_t kWriteThenFlowPoll[] = {
  0x54, 0xab,
  0x82, 0x01,
  0x07,        // payload size
    0x01,      // write single int8
    0x00,      // register 0
    0x00,      // value 0
    0x44,      // flow poll
    0x09,      // channel 9
    0x00,      // packet_number 0
    0x05,      // max bytes 5
  0x24, 0x5e,  // CRC
  0x00,
};
}

BOOST_FIXTURE_TEST_CASE(FlowPollTruncatedTest, Fixture) {
  // Each truncated frame should increment malformed_subframe.
  const auto initial_malformed = dut.stats()->malformed_subframe;

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollTruncJustType),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(dut.stats()->malformed_subframe == initial_malformed + 1);

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollTruncNoPacketNum),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(dut.stats()->malformed_subframe == initial_malformed + 2);

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollTruncNoMaxBytes),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(dut.stats()->malformed_subframe == initial_malformed + 3);
}

BOOST_FIXTURE_TEST_CASE(FlowPollBadChannelTest, Fixture) {
  const auto initial_malformed = dut.stats()->malformed_subframe;

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollBadChannel),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(dut.stats()->malformed_subframe == initial_malformed + 1);
}

BOOST_FIXTURE_TEST_CASE(FlowPollNoDataTest, Fixture) {
  // Poll with flow control when no data has been queued by the app.
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();
  BOOST_TEST(read_count == 1);

  // Response should have packet_number=1 and 0 bytes of data.
  const uint8_t kExpectedEmpty[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x04,        // payload size
     0x43,
      0x09,      // channel 9
      0x01,      // packet_number
      0x00,      // 0 bytes
    0x3b, 0x3a,  // CRC
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedEmpty));
}

BOOST_FIXTURE_TEST_CASE(FlowPollZeroMaxBytesTest, Fixture) {
  // Queue data, but client requests max_bytes=0.
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollZeroMax),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();
  BOOST_TEST(read_count == 1);
  // Write callback not invoked: zero bytes were sent.
  BOOST_TEST(write_count == 0);

  // Response should have packet_number=1 and 0 bytes.
  const uint8_t kExpectedZero[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x04,
     0x43,
      0x09,
      0x01,      // packet_number
      0x00,      // 0 bytes
    0x3b, 0x3a,  // CRC
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedZero));

  // Now poll again with proper max_bytes — data should still be available.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Ack packet 1 (which had 0 bytes, so nothing to retransmit).
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlowAck1),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 0);

  // Should get data now with packet_number=2.
  const uint8_t kExpectedData[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x09,        // payload size
     0x43,
      0x09,
      0x02,      // packet_number 2
      0x05,      // 5 bytes
      's', 't', 'u', 'f', 'f',
    0xca, 0x8e,  // CRC
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedData));
}

BOOST_FIXTURE_TEST_CASE(FlowPollNoResponseRequestedTest, Fixture) {
  // Send a flow poll from a source that does not request a response.
  // Flow state should not be affected.
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
      });

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollNoResponse),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  // No response was generated (source bit 7 not set).  Now send a real
  // flow poll with response requested — should behave as if the first
  // poll never happened.
  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();
  BOOST_TEST(read_count == 1);

  // Normal response expected: packet_number=1, 5 bytes "stuff"
  const uint8_t kExpected[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x09,
     0x43,
      0x09,
      0x01,
      0x05,
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpected));
}

BOOST_FIXTURE_TEST_CASE(FlowPollWildAckWhilePendingTest, Fixture) {
  // Queue data, send initial flow poll, then send a poll with a wildly
  // wrong packet_number (255).  Server should retransmit.
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Initial poll: server sends data with pkt=1.
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);

  // Now send poll with packet_number=255 (wildly wrong).
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollAck255),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);

  // Should retransmit the same data.
  const uint8_t kExpectedRetransmit[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x09,
     0x43,
      0x09,
      0x01,      // same packet_number
      0x05,
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedRetransmit));
}

BOOST_FIXTURE_TEST_CASE(FlowPollAckThenZeroMaxTest, Fixture) {
  // Queue data, get initial response, ack it with max_bytes=0.
  // Verify the ack releases old data but no new data is sent.
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Initial poll.
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 0);

  // Ack with packet_number=1 but max_bytes=0.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kFlowPollAck1Max0),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  // Ack should have released the write callback.
  BOOST_TEST(write_count == 1);

  // Response: packet_number=2, 0 bytes (max was 0).
  const uint8_t kExpectedAckZero[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x04,
     0x43,
      0x09,
      0x02,      // packet_number
      0x00,      // 0 bytes
    0x68, 0x6f,  // CRC
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedAckZero));
}

BOOST_FIXTURE_TEST_CASE(FlowPollMixWithOldPollTest, Fixture) {
  // Mixing old-style 0x42 polls with 0x44 flow polls on the same
  // channel.  The old poll consumes the buffer and resets flow state
  // so the server remains internally consistent.
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Step 1: Flow poll — data sent but held until ack.
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 0);  // Buffer held.

  // Step 2: Old-style 0x42 poll on the same channel.  It consumes
  // the write buffer, fires the callback, and resets flow state.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPoll),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 1);

  const uint8_t kExpectedOldResponse[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x08,
     0x41,       // kServerToClient (old style)
      0x09,
      0x05,
      's', 't', 'u', 'f', 'f',
    0xc5, 0xa8,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedOldResponse));

  // Step 3: Subsequent flow poll.  flow_pending_ was reset by the
  // old poll, so the server treats this as a fresh request with no
  // data available.  Packet number still advances.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);

  // Clean response: packet_number=2, 0 bytes (no data pending).
  const uint8_t kExpectedClean[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x04,
     0x43,
      0x09,
      0x02,      // packet_number advanced
      0x00,      // 0 bytes — no data
    0x68, 0x6f,  // CRC
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedClean));
}

BOOST_FIXTURE_TEST_CASE(FlowPollWriteSubframeComboTest, Fixture) {
  // Frame containing a register write subframe followed by a flow poll.
  // Both should be processed correctly.
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kWriteThenFlowPoll),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();

  // Register write should have been processed.
  BOOST_TEST(server.writes_.size() == 1);
  BOOST_TEST(server.writes_.at(0).reg == 0);

  BOOST_TEST(read_count == 1);
  // Buffer held until ack.
  BOOST_TEST(write_count == 0);

  // Response should contain the flow data (write had no error).
  const uint8_t kExpectedCombo[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x09,
     0x43,
      0x09,
      0x01,
      0x05,
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedCombo));
}

BOOST_FIXTURE_TEST_CASE(FlowPollMultipleRetransmitsTest, Fixture) {
  // Verify that the server retransmits the same data repeatedly when
  // the client keeps sending the wrong packet_number.
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
      });

  char receive_buffer[256] = {};
  auto do_poll = [&](const uint8_t* frame_data, size_t frame_size) {
    int read_count = 0;
    ssize_t read_size = 0;
    dut_stream.side_a()->AsyncReadSome(
        receive_buffer, [&](micro::error_code ec, ssize_t size) {
          BOOST_TEST(!ec);
          read_count++;
          read_size = size;
        });

    AsyncWrite(*dut_stream.side_a(),
               std::string_view(reinterpret_cast<const char*>(frame_data),
                                frame_size),
               [](micro::error_code ec) { BOOST_TEST(!ec); });
    Poll();
    BOOST_TEST(read_count == 1);
    return read_size;
  };

  const uint8_t kExpectedData[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x09,
     0x43, 0x09, 0x01, 0x05,
      's', 't', 'u', 'f', 'f',
    0x48, 0x56,
    0x00,
  };

  // Initial poll.
  auto sz = do_poll(kClientToServerPollFlow, sizeof(kClientToServerPollFlow));
  BOOST_TEST(std::string_view(receive_buffer, sz) == str(kExpectedData));

  // Retransmit 1 (wrong ack: 0).
  sz = do_poll(kClientToServerPollFlow, sizeof(kClientToServerPollFlow));
  BOOST_TEST(std::string_view(receive_buffer, sz) == str(kExpectedData));

  // Retransmit 2 (wild ack: 255).
  sz = do_poll(kFlowPollAck255, sizeof(kFlowPollAck255));
  BOOST_TEST(std::string_view(receive_buffer, sz) == str(kExpectedData));

  // Retransmit 3 (wrong ack: 0 again).
  sz = do_poll(kClientToServerPollFlow, sizeof(kClientToServerPollFlow));
  BOOST_TEST(std::string_view(receive_buffer, sz) == str(kExpectedData));
}

BOOST_FIXTURE_TEST_CASE(FlowPollMixClientToServerDataTest, Fixture) {
  // Send 0x40 (client-to-server data with response) while flow data
  // is pending.  The 0x40 handler consumes the buffer and resets flow
  // state so the server remains consistent.
  int write_count = 0;
  tunnel->AsyncWriteSome(
      "stuff to test",
      [&](micro::error_code ec, ssize_t) {
        BOOST_TEST(!ec);
        write_count++;
      });

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  // Flow poll — holds data.
  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 0);

  // Now send 0x40 with empty data on same channel.  The 0x40 handler
  // consumes write_buffer_, resets flow state, and fires the callback.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerEmpty),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);
  BOOST_TEST(write_count == 1);

  // 0x40 response sent all 13 bytes via 0x41.
  const uint8_t kExpected40Response[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x10,        // payload size
     0x41,
      0x09,
      0x0d,      // 13 bytes
      's', 't', 'u', 'f', 'f', ' ', 't', 'o', ' ', 't', 'e', 's', 't',
    0x9d, 0xd2,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpected40Response));

  // Subsequent flow poll: flow state was reset, so server sends a
  // clean empty response with advanced packet number.
  read_count = 0;
  read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPollFlow),
             [](micro::error_code ec) { BOOST_TEST(!ec); });
  Poll();
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedClean[] = {
    0x54, 0xab,
    0x01, 0x02,
    0x04,
     0x43,
      0x09,
      0x02,      // packet_number advanced
      0x00,      // 0 bytes
    0x68, 0x6f,
    0x00,
  };

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             str(kExpectedClean));
}

BOOST_FIXTURE_TEST_CASE(ResponseBufferOverflowTest, Fixture) {
  // A crafted frame: 84 unknown-register int8 writes (each emits a
  // 3-byte write-error response) followed by a 0x42 ClientPollServer
  // subframe.  Pre-fix the trailing tunnel handler would emit a 5-byte
  // varuint computed from a negative remaining-space arithmetic,
  // walking past the end of the 256-byte response buffer and tripping
  // the BufferWriteStream::skip assertion.  Post-fix ProcessSubframes
  // bails out once the response buffer no longer has headroom for any
  // subframe.
  server.next_write_error_ = MicroServer::Server::kUnknownRegister;

  constexpr int kNumWrites = 84;

  std::vector<uint8_t> payload;
  payload.push_back(0x00);          // write multiple int8
  payload.push_back(kNumWrites);    // count varuint (< 128)
  payload.push_back(0x01);          // start register 1
  for (int i = 0; i < kNumWrites; i++) {
    payload.push_back(0x00);        // value
  }
  payload.push_back(0x42);          // kClientPollServer
  payload.push_back(0x09);          // channel 9
  payload.push_back(0x00);          // max_bytes = 0

  std::vector<uint8_t> frame;
  frame.push_back(0x54);            // header LSB
  frame.push_back(0xab);            // header MSB
  frame.push_back(0x82);            // source = 2 with response bit
  frame.push_back(0x01);            // destination = 1
  BOOST_TEST_REQUIRE(payload.size() < 128);
  frame.push_back(static_cast<uint8_t>(payload.size()));
  frame.insert(frame.end(), payload.begin(), payload.end());

  boost::crc_ccitt_type crc;
  crc.process_bytes(frame.data(), frame.size());
  const uint16_t crc_value = crc.checksum();
  frame.push_back(crc_value & 0xff);
  frame.push_back((crc_value >> 8) & 0xff);

  AsyncWrite(*dut_stream.side_a(),
             std::string_view(reinterpret_cast<const char*>(frame.data()),
                              frame.size()),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  // Pre-fix: this Poll() would assert inside BufferWriteStream::skip.
  Poll();

  // Bail-out fired at least once for this frame.
  BOOST_TEST(dut.stats()->response_buffer_full > 0u);
}

BOOST_FIXTURE_TEST_CASE(ReadResponseCappedToTransportMaxSize, Fixture) {
  // ProcessFrame caps the response BufferWriteStream view to
  // min(buffer_size, datagram_properties_.max_size).  The test
  // fixture's MicroStreamDatagram reports max_size = 115, so a
  // multi-register float read large enough to need >115 bytes must
  // be aborted by ProcessSubframeRead with a kReadError rather than
  // emitted past the cap (which on FDCan would overflow the
  // transport's fixed-size staging buffer).
  for (uint32_t i = 0; i < 30; i++) {
    server.float_values[i] = static_cast<float>(i);
  }

  char receive_buffer[256] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  dut_stream.side_a()->AsyncReadSome(
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  Poll();

  std::string payload;
  payload.push_back(static_cast<char>(0x1c));  // kReadFloat, encoded_length=0
  payload.push_back(static_cast<char>(30));    // num_registers varuint
  payload.push_back(static_cast<char>(0));     // start_register varuint

  Frame frame{0x02, true, 0x01, payload};
  const std::string encoded = frame.encode();

  AsyncWrite(*dut_stream.side_a(),
             std::string_view(encoded.data(), encoded.size()),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();
  BOOST_TEST(read_count == 1);

  // The reply would be 1 (subframe_id) + 1 (num_registers varuint) +
  // 1 (start_register varuint) + 30 * 4 = 123 bytes, exceeding the
  // 115-byte cap, so ProcessSubframeRead aborts and emits a single
  // kReadError for the start register with error code 2.
  std::string expected_payload;
  expected_payload.push_back(static_cast<char>(0x31));  // kReadError
  expected_payload.push_back(static_cast<char>(0));     // start register
  expected_payload.push_back(static_cast<char>(2));     // error
  Frame expected_frame{0x01, false, 0x02, expected_payload};
  const std::string expected_encoded = expected_frame.encode();

  BOOST_TEST(std::string_view(receive_buffer, read_size) ==
             std::string_view(expected_encoded));
  BOOST_TEST(dut.stats()->response_buffer_full > 0u);
}

BOOST_FIXTURE_TEST_CASE(ReceiveOverrunReinterpretsAsSubframes, Fixture) {
  // No tunnel reader is posted, so kClientToServer data accumulates
  // in tunnel.read_data_ (capacity 128).  A second kClientToServer
  // that exceeds the remaining capacity used to skip the data-byte
  // consume, causing the next ProcessSubframes iteration to re-parse
  // those bytes as new subframes — turning a tunnel overrun into a
  // back-channel for register RPCs.
  std::string payload;
  payload.push_back(static_cast<char>(0x40));  // kClientToServer
  payload.push_back(static_cast<char>(0x09));  // channel 9
  payload.push_back(static_cast<char>(0x80));  // varuint 128 lsb
  payload.push_back(static_cast<char>(0x01));  // varuint 128 msb
  payload.append(128, 'A');
  payload.push_back(static_cast<char>(0x40));  // kClientToServer
  payload.push_back(static_cast<char>(0x09));  // channel 9
  payload.push_back(static_cast<char>(0x05));  // 5 bytes (overrun)
  payload.push_back(static_cast<char>(0x01));  // would-be kWriteInt8
  payload.push_back(static_cast<char>(0x00));  // would-be register 0
  payload.push_back(static_cast<char>(0x42));  // would-be value
  payload.push_back(static_cast<char>(0x50));  // kNop
  payload.push_back(static_cast<char>(0x50));  // kNop

  Frame frame{0x02, false, 0x01, payload};
  const std::string encoded = frame.encode();

  AsyncWrite(*dut_stream.side_a(),
             std::string_view(encoded.data(), encoded.size()),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  Poll();

  BOOST_TEST(dut.stats()->receive_overrun == 1u);
  // No spurious register write should be observed.
  BOOST_TEST(server.writes_.empty());
}
