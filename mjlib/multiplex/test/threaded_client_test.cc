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


#include "mjlib/multiplex/threaded_client.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/system_error.h"

using mjlib::multiplex::ThreadedClient;
namespace base = mjlib::base;
namespace mp = mjlib::multiplex;

namespace {
void ThrowIf(bool value) {
  if (value) {
    throw base::system_error::syserrno("");
  }
}

struct Pipe {
  int fd[2] = {};

  Pipe() {
    ThrowIf(::socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0);
  }

  ~Pipe() {
    ::close(fd[0]);
    ::close(fd[1]);
  }
};
}

BOOST_AUTO_TEST_CASE(ThreadedClientBasic) {
  Pipe pipe;
  ThreadedClient::Options dut_options;
  dut_options.fd = pipe.fd[1];

  boost::asio::io_context service;
  auto poll = [&]() {
    service.poll();
    service.reset();
  };
  ThreadedClient dut(service, dut_options);

  ThreadedClient::Request request;
  ThreadedClient::Reply reply;

  request.requests.resize(3);
  request.requests[0].request.ReadSingle(0, 0);
  request.requests[0].id = 2;

  request.requests[1].request.ReadSingle(1, 0);
  request.requests[1].id = 3;

  request.requests[2].request.ReadSingle(2, 0);
  request.requests[2].id = 4;

  int done = 0;
  dut.AsyncRegister(&request, &reply, [&](auto ec) {
      BOOST_TEST(!ec);
      done++;
    });

  poll();
  BOOST_TEST(done == 0);

  // We should be able to read the first request now.
  char buf[4096] = {};
  {
    const int result = ::read(pipe.fd[0], buf, 9);
    BOOST_TEST(result == 9);
    BOOST_TEST(std::string_view(buf, 9) ==
               std::string("\x54\xab\x80\x02\x02\x11\x00\x8f\x69", 9));
  }

  // Send our reply.
  {
    const int result =
        ::write(pipe.fd[0], "\x54\xab\x02\x00\x03\x21\x00\x05\xd8\x8a", 10);
    BOOST_TEST(result == 10);
  }

  // Now we should have another request.
  {
    const int result = ::read(pipe.fd[0], buf, 9);
    BOOST_TEST(result == 9);
    BOOST_TEST(std::string_view(buf, 9) ==
               std::string("\x54\xab\x80\x03\x02\x11\x01\x1a\x0f", 9));
  }

  // Send our reply.
  {
    const int result =
        ::write(pipe.fd[0], "\x54\xab\x03\x00\x03\x21\x01\x06\x2a\xcc", 10);
    BOOST_TEST(result == 10);
  }

  // Get our final request.
  {
    const int result = ::read(pipe.fd[0], buf, 9);
    BOOST_TEST(result == 9);
    BOOST_TEST(std::string_view(buf, 9) ==
               std::string("\x54\xab\x80\x04\x02\x11\x02\x54\x6e", 9));
  }

  // And send our final reply.
  {
    const int result =
        ::write(pipe.fd[0], "\x54\xab\x04\x00\x03\x21\x02\x07\x19\x41", 10);
    BOOST_TEST(result == 10);
  }

  BOOST_TEST(done == 0);
  // Unfortunately, this is synchronizing with a blocking bg thread.
  // We'll just sleep to give it a chance to work.
  ::usleep(100000);

  poll();
  BOOST_TEST(done == 1);

  // Now look to see if the reply is correct.
  BOOST_TEST(reply.replies.size() == 3);
  BOOST_TEST(reply.replies[0].id == 2);
  BOOST_TEST((reply.replies[0].reply.at(0) ==
              mp::Format::ReadResult(
                  mp::Format::Value(static_cast<int8_t>(5)))));

  BOOST_TEST(reply.replies[1].id == 3);
  BOOST_TEST((reply.replies[1].reply.at(1) ==
              mp::Format::ReadResult(
                  mp::Format::Value(static_cast<int8_t>(6)))));

  BOOST_TEST(reply.replies[2].id == 4);
  BOOST_TEST((reply.replies[2].reply.at(2) ==
              mp::Format::ReadResult(
                  mp::Format::Value(static_cast<int8_t>(7)))));
}
