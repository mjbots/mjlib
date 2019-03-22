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

#include "mjlib/io/streambuf_read_stream.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::io::StreambufReadStream;

BOOST_AUTO_TEST_CASE(BufferReadStreamEmptyTest) {
  boost::asio::streambuf streambuf;
  StreambufReadStream dut{&streambuf};
  BOOST_TEST(dut.remaining() == 0);
  BOOST_TEST(dut.offset() == 0);

  dut.ignore(1);
  BOOST_TEST(dut.gcount() == 0);
  BOOST_TEST(dut.offset() == 0);
  BOOST_TEST(dut.remaining() == 0);
}

BOOST_AUTO_TEST_CASE(BufferReadStreamTest) {
  boost::asio::streambuf streambuf;
  std::ostream ostr(&streambuf);
  ostr.write("hello there", 11);

  StreambufReadStream dut{&streambuf};
  BOOST_TEST(dut.remaining() == 11);
  BOOST_TEST(dut.offset() == 0);

  dut.ignore(1);
  BOOST_TEST(dut.gcount() == 1);
  BOOST_TEST(dut.offset() == 1);
  BOOST_TEST(dut.remaining() == 10);

  char buf[4] = {};
  dut.read(buf);
  BOOST_TEST(dut.gcount() == 4);
  BOOST_TEST(dut.offset() == 5);
  BOOST_TEST(dut.remaining() == 6);
  BOOST_TEST(std::string(buf, 4) == "ello");
}
