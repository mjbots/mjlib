// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/rs485_frame_stream.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/stream_pipe_factory.h"
#include "mjlib/io/test/reader.h"

namespace io = mjlib::io;
using mjlib::multiplex::Frame;
using mjlib::multiplex::Rs485FrameStream;

namespace {
struct Fixture {
  void Poll() {
    context.poll();
    context.reset();
  }

  boost::asio::io_context context;
  io::StreamPipeFactory pipe_factory{context.get_executor()};
  io::SharedStream client_side{pipe_factory.GetStream("", 1)};
  Rs485FrameStream dut{context.get_executor(), {}, client_side.get()};

  io::SharedStream server_side{pipe_factory.GetStream("", 0)};
  io::test::Reader server_reader{server_side.get()};
};
}

BOOST_FIXTURE_TEST_CASE(Rs485FrameStreamWriteTest, Fixture) {
  Frame to_send;
  to_send.source_id = 1;
  to_send.dest_id = 2;
  to_send.request_reply = false;

  int write_done = 0;
  dut.AsyncWrite(&to_send, [&](const mjlib::base::error_code& ec) {
      mjlib::base::FailIf(ec);
      write_done++;
    });

  BOOST_TEST(write_done == 0);
  BOOST_TEST(server_reader.data().empty());

  Poll();

  BOOST_TEST(write_done == 1);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x01\x02\x00\x03\x28", 7));
}

BOOST_FIXTURE_TEST_CASE(Rs485FrameStreamWriteMultipleTest, Fixture) {
  Frame to_send1;
  Frame to_send2;
  to_send1.source_id = 1;
  to_send1.dest_id = 2;
  to_send1.request_reply = false;

  to_send2.source_id = 4;
  to_send2.dest_id = 5;
  to_send2.request_reply = false;

  const std::vector<const Frame*> frames = {&to_send1, &to_send2};

  int write_done = 0;
  dut.AsyncWriteMultiple(frames, [&](const mjlib::base::error_code& ec) {
      mjlib::base::FailIf(ec);
      write_done++;
    });

  BOOST_TEST(write_done == 0);
  Poll();

  BOOST_TEST(write_done == 1);
  const auto actual = server_reader.data();
  const auto expected =
      std::string(
          "\x54\xab\x01\x02\x00\x03\x28\x54\xab\x04\x05\x00\x64\x5a", 14);
  BOOST_TEST(actual == expected);
}

BOOST_FIXTURE_TEST_CASE(Rs485FrameStreamReadTest, Fixture) {
  Frame to_receive;
  int read_done = 0;

  dut.AsyncRead(&to_receive, {}, [&](auto&& ec) {
      mjlib::base::FailIf(ec);
      read_done++;
    });

  BOOST_TEST(read_done == 0);
  Poll();
  BOOST_TEST(read_done == 0);

  int write_done = 0;
  // Now write a frame into the server side.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("\x54\xab\x04\x05\x01\x20\xec\x88", 8),
      [&](auto&& ec, size_t size) {
        mjlib::base::FailIf(ec);
        write_done++;
        BOOST_TEST(size == 8);
      });

  BOOST_TEST(write_done == 0);

  Poll();
  BOOST_TEST(read_done == 1);
  BOOST_TEST(write_done == 1);

  BOOST_TEST(to_receive.source_id == 4);
  BOOST_TEST(to_receive.dest_id == 5);
  BOOST_TEST(to_receive.payload == " ");
}

BOOST_FIXTURE_TEST_CASE(Rs485FrameStreamReadCancelTest, Fixture) {
  Frame to_receive;
  int read_done = 0;

  dut.AsyncRead(&to_receive, {}, [&](auto&& ec) {
      BOOST_TEST(ec == boost::asio::error::operation_aborted);
      read_done++;
    });

  BOOST_TEST(read_done == 0);
  Poll();
  BOOST_TEST(read_done == 0);

  // Cancel this.
  dut.cancel();
  Poll();

  BOOST_TEST(read_done == 1);

  int write_done = 0;
  auto write = [&]() {
    // Now write a frame into the server side.
    boost::asio::async_write(
        *server_side,
        boost::asio::buffer("\x54\xab\x04\x05\x01\x20\xec\x88", 8),
        [&](auto&& ec, size_t size) {
          mjlib::base::FailIf(ec);
          write_done++;
          BOOST_TEST(size == 8);
        });
  };

  // Writing should not result in any more reads.
  write();
  Poll();
  BOOST_TEST(write_done == 1);
  BOOST_TEST(read_done == 1);

  // But we can read another time and get something new.
  int read_done2 = 0;
  dut.AsyncRead(&to_receive, {}, [&](auto&& ec) {
      mjlib::base::FailIf(ec);
      read_done2++;
    });

  Poll();
  write();
  Poll();

  BOOST_TEST(read_done2 == 1);
}
