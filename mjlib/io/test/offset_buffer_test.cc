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

#include "mjlib/io/offset_buffer.h"

#include <string>

#include <boost/test/auto_unit_test.hpp>

using mjlib::io::OffsetBufferSequence;

BOOST_AUTO_TEST_CASE(OffsetBufferSequenceTest) {
  auto offset = OffsetBufferSequence(boost::asio::buffer("hello", 5), 1);
  char data[6] = {};
  const size_t copied = boost::asio::buffer_copy(boost::asio::buffer(data), offset);
  BOOST_TEST(copied == 4);
  BOOST_TEST(std::string(data, 4) == "ello");
}
