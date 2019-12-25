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

#include "mjlib/base/buffer_stream.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/string_span.h"

BOOST_AUTO_TEST_CASE(BufferReadStreamTest) {
  const char data[] = "abcdef";
  {
    mjlib::base::BufferReadStream dut1(data);
    BOOST_TEST(dut1.offset() == 0);
    BOOST_TEST(dut1.remaining() == 6);
    BOOST_TEST(dut1.size() == 6);

    char read[10] = {};
    dut1.read(mjlib::base::string_span(read, 2));
    BOOST_TEST(dut1.offset() == 2);
    BOOST_TEST(dut1.remaining() == 4);
    BOOST_TEST(dut1.size() == 6);
    BOOST_TEST(dut1.gcount() == 2);

    dut1.read(mjlib::base::string_span(read, 6));
    BOOST_TEST(dut1.offset() == 6);
    BOOST_TEST(dut1.remaining() == 0);
    BOOST_TEST(dut1.size() == 6);
    BOOST_TEST(dut1.gcount() == 4);
  }
}

BOOST_AUTO_TEST_CASE(BufferWriteStreamTest) {
  char buf[64] = {};

  {
    mjlib::base::BufferWriteStream dut{mjlib::base::string_span(buf)};
    mjlib::base::WriteStream::Iterator it{dut};
    *it = 't';
    ++it;
    *it = 'e';
    ++it;
    *it = 's';
    ++it;
    *it = 't';
    ++it;

    BOOST_TEST(std::string(buf, 4) == "test");
  }
}
