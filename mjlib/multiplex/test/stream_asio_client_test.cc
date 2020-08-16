// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/stream_asio_client.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/debug_deadline_service.h"
#include "mjlib/io/stream_pipe_factory.h"
#include "mjlib/io/test/reader.h"

#include "mjlib/multiplex/rs485_frame_stream.h"

namespace base = mjlib::base;
namespace io = mjlib::io;
namespace mp = mjlib::multiplex;
using mp::StreamAsioClient;

namespace {
struct Fixture {
  void Poll() {
    context.poll();
    context.reset();
  }

  void PollOne() {
    context.poll_one();
    context.reset();
  }

  void AsyncRegister(const mp::RegisterRequest& request_in) {
    request = {{2, request_in}};
    reply = {};

    dut.AsyncTransmit(
        &request,
        &reply,
        [this](const base::error_code& ec) {
          mjlib::base::FailIf(ec);
          this->register_done++;
        });
  }

  boost::asio::io_context context;
  boost::asio::any_io_executor executor{context.get_executor()};
  io::DebugDeadlineService* const debug_service{
    io::DebugDeadlineService::Install(context)};
  io::StreamPipeFactory pipe_factory{executor};
  io::SharedStream client_side{pipe_factory.GetStream("", 1)};
  mp::Rs485FrameStream frame_stream{context.get_executor(), {}, client_side.get()};
  StreamAsioClient dut{&frame_stream};

  io::SharedStream server_side{pipe_factory.GetStream("", 0)};
  io::test::Reader server_reader{server_side.get()};

  int register_done = 0;

  mp::AsioClient::Request request;
  mp::AsioClient::Reply reply;
};
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientRegisterNoReply, Fixture) {
  mp::RegisterRequest request;
  request.WriteSingle(1, static_cast<int8_t>(10));

  AsyncRegister(request);

  BOOST_TEST(register_done == 0);

  Poll();

  BOOST_TEST(register_done == 1);
  BOOST_TEST(reply.size() == 0);
  BOOST_TEST(server_reader.data().size() == 10u);
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientRegisterReply, Fixture) {
  mp::RegisterRequest request;
  request.ReadSingle(3, 0);

  AsyncRegister(request);

  BOOST_TEST(register_done == 0);
  Poll();

  BOOST_TEST(register_done == 0);
  BOOST_TEST(server_reader.data().size() == 9u);

  // Write the device's reply.
  int write_done = 0;
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("\x54\xab\x02\x00\x03\x21\x03\x04\xaa\xcf", 10),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
        write_done++;
      });

  BOOST_TEST(write_done == 0);

  Poll();
  BOOST_TEST(write_done == 1);
  BOOST_TEST(register_done == 1);
  BOOST_TEST(reply.size() == 1);
  const auto it = std::find_if(reply.begin(), reply.end(),
                               [](const auto& v) { return v.reg == 3; });
  BOOST_TEST((it != reply.end()));
  BOOST_TEST((it->value == mp::Format::ReadResult(
                  mp::Format::Value(static_cast<int8_t>(4)))));
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelWrite, Fixture) {
  auto tunnel = dut.MakeTunnel(2, 3);

  int write_done = 0;
  size_t size = 0;
  tunnel->async_write_some(
      boost::asio::buffer("hello", 5),
      [&](auto&& ec, size_t size_in) {
        base::FailIf(ec);
        write_done++;
        size = size_in;
      });

  BOOST_TEST(write_done == 0);
  Poll();
  BOOST_TEST(write_done == 1);
  BOOST_TEST(size == 5u);
  BOOST_TEST(server_reader.data().size() == 15u);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x00\x02\x08\x40\x03\x05hello\xb7\xe2", 15));
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelRead, Fixture) {
  auto tunnel = dut.MakeTunnel(2, 3);

  char buf[10] = {};
  int read_done = 0;
  size_t size = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf),
      [&](auto&&ec, size_t size_in) {
        base::FailIf(ec);
        read_done++;
        size = size_in;
      });

  BOOST_TEST(read_done == 0);

  Poll();

  BOOST_TEST(read_done == 0);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38", 10));

  // Now respond with some data.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("\x54\xab\x02\x00\x05\x41\x03\x02\x61\x62\xa8\x40", 12),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
      });

  Poll();

  BOOST_TEST(read_done == 1);
  BOOST_TEST(size == 2);
  BOOST_TEST(std::string(buf, 2) == "ab");
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelReadCancel, Fixture) {
  auto tunnel = dut.MakeTunnel(2, 3);

  char buf[10] = {};
  int read_done = 0;
  size_t size = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf),
      [&](auto&&ec, size_t size_in) {
        BOOST_TEST(!!ec);
        read_done++;
        size = size_in;
      });

  BOOST_TEST(read_done == 0);

  Poll();

  BOOST_TEST(read_done == 0);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38", 10));

  tunnel->cancel();
  debug_service->SetTime(
      debug_service->now() + boost::posix_time::milliseconds(100));

  Poll();

  BOOST_TEST(read_done == 1);

  // Now respond with some data.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("\x54\xab\x02\x00\x05\x41\x03\x02\x61\x62\xa8\x40", 12),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
      });

  Poll();

  BOOST_TEST(read_done == 1);
  BOOST_TEST(buf[0] == 0);
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelReadCancelRace, Fixture) {
  auto tunnel = dut.MakeTunnel(2, 3);

  char buf[10] = {};
  int read_done = 0;
  size_t size = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf),
      [&](auto&&ec, size_t size_in) {
        BOOST_TEST(!!ec);
        read_done++;
        size = size_in;
      });

  BOOST_TEST(read_done == 0);

  // Poll until just when data appears... this means that not all
  // callbacks will have been invoked internally.
  while (server_reader.data().size() != 10) {
    PollOne();
  }

  tunnel->cancel();

  // We should be good and canceled, so should be safe to read again.
  char buf2[10] = {};
  int read_done2 = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf2),
      [&](auto&& ec, size_t) {
        BOOST_TEST(!!ec);
        read_done2++;
      });

  // Now respond with some data.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer("\x54\xab\x02\x00\x05\x41\x03\x02\x61\x62\xa8\x40", 12),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
      });

  Poll();

  // It is undefined whether the new data gets into the new buffer or
  // not.  But what definitely *shouldn't* happen is for the new data
  // to get into the old buffer, since we canceled the read before the
  // data was there.
  BOOST_TEST(read_done == 1);
  BOOST_TEST(buf[0] == 0);

  BOOST_TEST(((read_done2 == 0) || (read_done2 == 1)));
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelReadFlush, Fixture) {
  auto tunnel = dut.MakeTunnel(2, 3);

  // Assume the device had some timing issues and sent a bunch of
  // replies all at once.
  char buf[10] = {};
  int read_done = 0;
  size_t size = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf),
      [&](auto&&ec, size_t size_in) {
        base::FailIf(ec);
        read_done++;
        size = size_in;
      });

  BOOST_TEST(read_done == 0);

  Poll();

  BOOST_TEST(read_done == 0);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38", 10));

  // Now respond with some data.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer(
          "\x54\xab\x02\x00\x04\x41\x03\x01\x63\x95\x1f"
          "\x54\xab\x02\x00\x03\x41\x03\x00\x45\x14"
          "\x54\xab\x02\x00\x05\x41\x03\x02\x61\x62\xa8\x40"
          "\x54\xab\x02\x00\x03\x41\x03\x00\x45\x14",
          43),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
      });

  Poll();

  BOOST_TEST(read_done == 1);
  BOOST_TEST(size == 3);
  BOOST_TEST(std::string(buf, 3) == "cab");

  // And it shouldn't have emitted any more polls.
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38", 10));
}

BOOST_FIXTURE_TEST_CASE(StreamAsioClientTunnelReadFlushRace, Fixture) {
  // Here, if there were multiple tunnels outstanding, and a bunch of
  // data came in at once that needed a flush, we could assert because
  // the tunnel was calling read outside of the ExclusiveCommand lock.

  auto tunnel = dut.MakeTunnel(2, 3);
  auto tunnel2 = dut.MakeTunnel(2, 4);

  // Assume the device had some timing issues and sent a bunch of
  // replies all at once.
  char buf[10] = {};
  int read_done = 0;
  size_t size = 0;
  tunnel->async_read_some(
      boost::asio::buffer(buf),
      [&](auto&&ec, size_t size_in) {
        base::FailIf(ec);
        read_done++;
        size = size_in;
      });

  BOOST_TEST(read_done == 0);

  // Kick off the second one as well so that it gets in the queue.
  char buf2[10] = {};
  int read2_done = 0;
  size_t size2 = 0;
  tunnel2->async_read_some(
      boost::asio::buffer(buf2),
      [&](auto&&ec, size_t size_in) {
        base::FailIf(ec);
        read2_done++;
        size2 = size_in;
      });

  BOOST_TEST(read2_done == 0);

  Poll();

  BOOST_TEST(read_done == 0);
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38", 10));

  // Now respond with some data.
  boost::asio::async_write(
      *server_side,
      boost::asio::buffer(
          "\x54\xab\x02\x00\x04\x41\x03\x01\x63\x95\x1f"
          "\x54\xab\x02\x00\x04\x41\x03\x01\x64\x72\x6f"
          "\x54\xab\x02\x00\x05\x41\x03\x02\x61\x62\xa8\x40"
          "\x54\xab\x02\x00\x05\x41\x03\x02\x65\x66\xe8\xcc"
          "\x54\xab\x02\x00\x05\x41\x03\x02\x67\x68\x44\x4b"
          "\x54\xab\x02\x00\x05\x41\x03\x02\x69\x6a\x09\x48",
          70),
      [&](auto&& ec, size_t) {
        base::FailIf(ec);
      });

  Poll();

  BOOST_TEST(read_done == 1);
  BOOST_TEST(size == 10);
  BOOST_TEST(std::string(buf, 10) == "cdabefghij");

  // And now the second tunnel should have had a chance to send out a
  // poll.
  BOOST_TEST(server_reader.data() ==
             std::string("\x54\xab\x80\x02\x03\x40\x03\x00\x96\x38"
                         "\x54\xab\x80\x02\x03\x40\x04\x00\x01\xa1",
                         20));
}
