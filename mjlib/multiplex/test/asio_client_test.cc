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

#include "mjlib/multiplex/asio_client.h"

#include <boost/asio/write.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/io/debug_deadline_service.h"
#include "mjlib/io/stream_pipe_factory.h"
#include "mjlib/io/test/reader.h"

namespace base = mjlib::base;
namespace io = mjlib::io;
namespace mp = mjlib::multiplex;
using mp::AsioClient;

namespace {
struct Fixture {
  void Poll() {
    service.poll();
    service.reset();
  }

  void AsyncRegister(const mp::RegisterRequest& request) {
    dut.AsyncRegister(2, request, [this](const base::error_code& ec,
                                         const mp::RegisterReply& reply_in) {
                        mjlib::base::FailIf(ec);
                        this->register_done++;
                        this->reply = reply_in;
                      });
  }

  boost::asio::io_service service;
  io::DebugDeadlineService* const debug_service{
    io::DebugDeadlineService::Install(service)};
  io::StreamPipeFactory pipe_factory{service};
  io::SharedStream client_side{pipe_factory.GetStream("", 1)};
  AsioClient dut{client_side.get()};

  io::SharedStream server_side{pipe_factory.GetStream("", 0)};
  io::test::Reader server_reader{server_side.get()};

  int register_done = 0;
  mp::RegisterReply reply;
};
}

BOOST_FIXTURE_TEST_CASE(AsioClientRegisterNoReply, Fixture) {
  mp::RegisterRequest request;
  request.WriteSingle(1, static_cast<int8_t>(10));

  AsyncRegister(request);

  BOOST_TEST(register_done == 0);

  Poll();

  BOOST_TEST(register_done == 1);
  BOOST_TEST(reply.size() == 0);
  BOOST_TEST(server_reader.data().size() == 10u);
}

BOOST_FIXTURE_TEST_CASE(AsioClientRegisterReply, Fixture) {
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
  BOOST_TEST(reply.count(3) == 1);
  BOOST_TEST((reply.at(3) == mp::Format::ReadResult(
                  mp::Format::Value(static_cast<int8_t>(4)))));
}

BOOST_FIXTURE_TEST_CASE(AsioClientTunnelWrite, Fixture) {
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

BOOST_FIXTURE_TEST_CASE(AsioClientTunnelRead, Fixture) {
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

BOOST_FIXTURE_TEST_CASE(AsioClientTunnelReadCancel, Fixture) {
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
}
