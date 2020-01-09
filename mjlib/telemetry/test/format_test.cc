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

#include "mjlib/telemetry/format.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/fast_stream.h"

using namespace mjlib;

namespace {
struct WriteFixture {
  base::FastOStringStream ostr;
  telemetry::WriteStream dut{ostr};
};
}

BOOST_FIXTURE_TEST_CASE(WriteString, WriteFixture) {
  dut.WriteString("abc");
  BOOST_TEST(ostr.data()->size() == 4);
  BOOST_TEST(ostr.data()->data() == "\x03" "abc");
}

BOOST_FIXTURE_TEST_CASE(BoolWrite, WriteFixture) {
  dut.Write(true);
  BOOST_TEST(ostr.data()->size() == 1);
  BOOST_TEST(ostr.data()->data()[0] == 1);
}

BOOST_FIXTURE_TEST_CASE(FixedSizeWrite, WriteFixture) {
  dut.Write(static_cast<uint16_t>(123));
  BOOST_TEST(ostr.data()->size() == 2);
  BOOST_TEST(ostr.data()->data()[0] == 123);
  BOOST_TEST(ostr.data()->data()[1] == 0);
}

namespace {
struct VaruintFixture : WriteFixture {
  void Test(uint64_t value, const std::vector<uint8_t>& expected) {
    ostr = {};
    dut.WriteVaruint(value);
    BOOST_TEST(ostr.data()->size() == expected.size());
    BOOST_TEST(ostr.str() == std::string(
                   reinterpret_cast<const char*>(&expected[0]), expected.size()));
  }
};
}

BOOST_FIXTURE_TEST_CASE(WriteVaruint, VaruintFixture) {
  Test(0, {0});
  Test(1, {1});
  Test(128, {0x80, 0x01});
  Test(256, {0x80, 0x02});
  Test(257, {0x81, 0x02});
  Test(0xffffffffffffffff, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
}

namespace {
struct VarintFixture : WriteFixture {
  void Test(int64_t value, const std::vector<uint8_t>& expected) {
    ostr = {};
    dut.WriteVarint(value);
    BOOST_TEST(ostr.data()->size() == expected.size());
    BOOST_TEST(ostr.str() == std::string(
                   reinterpret_cast<const char*>(&expected[0]), expected.size()));
  }
};
}

BOOST_FIXTURE_TEST_CASE(WriteVarint, VarintFixture) {
  Test(0, {0});
  Test(-1, {1});
  Test(1, {2});
  Test(-2, {3});
  Test(2, {4});
  Test(-64, {0x7f});
  Test(64, {0x80, 0x01});
  Test(std::numeric_limits<int64_t>::min(),
       {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
  Test(std::numeric_limits<int64_t>::max(),
       {0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
}

BOOST_FIXTURE_TEST_CASE(WriteTimestamp, WriteFixture) {
  dut.Write(base::ConvertEpochMicrosecondsToPtime(100000000));
  BOOST_TEST(ostr.data()->size() == 8);
  auto found = *reinterpret_cast<const int64_t*>(
      reinterpret_cast<const char*>(&ostr.data()->data()[0]));
  BOOST_TEST(found == 100000000);
}


namespace {
struct ReadFixture {
  ReadFixture(const std::vector<uint8_t>& input)
      : input_data(reinterpret_cast<const char*>(&input[0]), input.size()) {}

  std::string input_data;
  base::FastIStringStream stream{input_data};
  telemetry::ReadStream dut{stream};
};
}

BOOST_AUTO_TEST_CASE(BasicRead) {
  {
    ReadFixture f({0x00});
    const auto maybe_found = f.dut.Read<uint8_t>();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 0);
  }
  {
    ReadFixture f({0x00});
    const auto maybe_found = f.dut.Read<bool>();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == false);
  }
  {
    ReadFixture f({0x01});
    const auto maybe_found = f.dut.Read<bool>();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == true);
  }
  {
    ReadFixture f({0x00, 0x01});
    const auto maybe_found = f.dut.Read<uint16_t>();
    BOOST_TEST(!!maybe_found);
    BOOST_TEST(*maybe_found == 256);
  }
}

BOOST_AUTO_TEST_CASE(ReadVaruint) {
  {
    ReadFixture f({0x00});
    const auto maybe_found = f.dut.ReadVaruint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 0);
  }
  {
    ReadFixture f({0x01});
    const auto maybe_found = f.dut.ReadVaruint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 1);
  }
  {
    ReadFixture f({0x80, 0x01});
    const auto maybe_found = f.dut.ReadVaruint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 128);
  }
  {
    ReadFixture f({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
    const auto maybe_found = f.dut.ReadVaruint();
    BOOST_TEST(!!maybe_found);
    BOOST_TEST(*maybe_found == std::numeric_limits<uint64_t>::max());
  }
}

BOOST_AUTO_TEST_CASE(ReadVarint) {
  {
    ReadFixture f({0x00});
    const auto maybe_found = f.dut.ReadVarint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 0);
  }
  {
    ReadFixture f({0x01});
    const auto maybe_found = f.dut.ReadVarint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == -1);
  }
  {
    ReadFixture f({0x02});
    const auto maybe_found = f.dut.ReadVarint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == 1);
  }
  {
    ReadFixture f({0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
    const auto maybe_found = f.dut.ReadVarint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == std::numeric_limits<int64_t>::max());
  }
  {
    ReadFixture f({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01});
    const auto maybe_found = f.dut.ReadVarint();
    BOOST_TEST_REQUIRE(!!maybe_found);
    BOOST_TEST(*maybe_found == std::numeric_limits<int64_t>::min());
  }
}

BOOST_AUTO_TEST_CASE(ReadString) {
  ReadFixture f({0x03, 'a', 'b', 'c'});
  const auto maybe_found = f.dut.ReadString();
  BOOST_TEST_REQUIRE(!!maybe_found);
  BOOST_TEST(*maybe_found == "abc");
}

BOOST_AUTO_TEST_CASE(ReadTimestamp) {
  ReadFixture f({0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00});
  const auto maybe_timestamp = f.dut.ReadTimestamp();
  BOOST_TEST_REQUIRE(!!maybe_timestamp);
  const auto timestamp = *maybe_timestamp;
  const auto found = base::ConvertPtimeToEpochMicroseconds(timestamp);
  BOOST_TEST(found == 0x100000);
}

BOOST_AUTO_TEST_CASE(ReadIgnore) {
  ReadFixture f({0x99, 0x03, 'a', 'b', 'c'});
  f.dut.Ignore(1);
  const auto maybe_found = f.dut.ReadString();
  BOOST_TEST_REQUIRE(!!maybe_found);
  BOOST_TEST(*maybe_found == "abc");
}
