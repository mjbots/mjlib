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

#include "mjlib/multiplex/fdcanusb_frame_stream.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/stream_pipe_factory.h"
#include "mjlib/io/test/reader.h"

namespace io = mjlib::io;
using mjlib::multiplex::Frame;
using mjlib::multiplex::FdcanusbFrameStream;

namespace {
struct Fixture {
  void Poll() {
    context.poll();
    context.reset();
  }

  boost::asio::io_context context;
  io::StreamPipeFactory pipe_factory{context.get_executor()};
  io::SharedStream client_side{pipe_factory.GetStream("", 1)};
  FdcanusbFrameStream dut{{}, client_side.get()};

  io::SharedStream server_side{pipe_factory.GetStream("", 0)};
  io::test::Reader server_reader{server_side.get()};
};
}

BOOST_FIXTURE_TEST_CASE(FdcanusbFrameStreamWriteTest, Fixture) {
  Frame to_send;
  to_send.source_id = 1;
  to_send.dest_id = 2;
  to_send.request_reply = false;
  to_send.payload = "a";

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
             "can send 102 61\n");
}

BOOST_FIXTURE_TEST_CASE(FdcanusbFrameStreamReadTest, Fixture) {
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
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("rcv 405 20\n", 11),
      [&](auto&& ec, size_t size) {
        mjlib::base::FailIf(ec);
        write_done++;
        BOOST_TEST(size == 11);
      });

  BOOST_TEST(write_done == 0);

  Poll();

  BOOST_TEST(read_done == 1);
  BOOST_TEST(write_done == 1);

  BOOST_TEST(to_receive.source_id == 4);
  BOOST_TEST(to_receive.dest_id == 5);
  BOOST_TEST(to_receive.payload == " ");
}
