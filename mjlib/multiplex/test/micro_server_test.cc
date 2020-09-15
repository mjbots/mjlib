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

#include "mjlib/multiplex/micro_server.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/stream_pipe.h"
#include "mjlib/micro/test/persistent_config_fixture.h"
#include "mjlib/micro/test/str.h"

#include "mjlib/multiplex/micro_stream_datagram.h"

namespace base = mjlib::base;
using namespace mjlib::multiplex;
using mjlib::micro::test::str;
namespace micro = mjlib::micro;
namespace test = micro::test;

namespace {
class Server : public MicroServer::Server {
 public:
  uint32_t Write(MicroServer::Register reg,
                 const MicroServer::Value& value) override {
    writes_.push_back({reg, value});
    return next_write_error_;
  }

  MicroServer::ReadResult Read(
      MicroServer::Register reg, size_t type_index) const override {
    if (type_index == 0) {
      return MicroServer::Value(int8_values.at(reg));
    } else if (type_index == 2) {
      return MicroServer::Value(int32_values.at(reg));
    } else if (type_index == 3) {
      return MicroServer::Value(float_values.at(reg));
    }
    return static_cast<uint32_t>(1);
  }

  struct WriteValue {
    MicroServer::Register reg;
    MicroServer::Value value;
  };

  std::vector<WriteValue> writes_;
  uint32_t next_write_error_ = 0;

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
    event_queue.Poll();
    BOOST_TEST(read_count == 0);

    {
      int write_count = 0;
      AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
                 [&](micro::error_code ec) {
                   BOOST_TEST(!ec);
                   write_count++;
                 });
      event_queue.Poll();
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
  event_queue.Poll();
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
  event_queue.Poll();
  BOOST_TEST(write_count == 1);

  char read_buffer[100] = {};
  int read_count = 0;
  ssize_t read_size = 0;
  tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
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
  tunnel->AsyncReadSome(read_buffer, [&](micro::error_code ec, ssize_t size) {
      BOOST_TEST(!ec);
      read_count++;
      read_size = size;
    });

  event_queue.Poll();
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServer),
             [&](micro::error_code ec) {
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
      [&](micro::error_code ec, ssize_t size) {
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
      receive_buffer, [&](micro::error_code ec, ssize_t size) {
        BOOST_TEST(!ec);
        read_count++;
        read_size = size;
      });

  event_queue.Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerEmpty),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

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

  event_queue.Poll();
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

  event_queue.Poll();
  BOOST_TEST(write_count == 0);
  BOOST_TEST(read_count == 0);

  AsyncWrite(*dut_stream.side_a(), str(kClientToServerPoll),
             [](micro::error_code ec) { BOOST_TEST(!ec); });

  event_queue.Poll();

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

  event_queue.Poll();
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

  event_queue.Poll();
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

  event_queue.Poll();
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

  event_queue.Poll();
  BOOST_TEST(read_count == 0);


  server.next_write_error_ = 0x76;

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kWriteSingle),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  event_queue.Poll();
  BOOST_TEST(write_count == 1);
  BOOST_TEST(read_count == 1);

  const uint8_t kExpectedResponse[] = {
    0x54, 0xab,
    0x01,  // source id
    0x02,  // dest id
    0x03,  // payload size
     0x30,  // write error
      0x02,  // register
      0x76,  // error
    0x7e, 0x5c,  // CRC
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

  event_queue.Poll();

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kReadSingle),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  event_queue.Poll();
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

  event_queue.Poll();

  int write_count = 0;
  AsyncWrite(*dut_stream.side_a(), str(kReadMultiple),
             [&](micro::error_code ec) {
               BOOST_TEST(!ec);
               write_count++;
             });

  event_queue.Poll();
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

  event_queue.Poll();
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

  event_queue.Poll();
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

  event_queue.Poll();
  BOOST_TEST(write_count == 1);

  // Nothing bad should have happened.
  BOOST_TEST(dut.stats()->unknown_subframe == 0);
}
