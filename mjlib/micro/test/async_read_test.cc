// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/micro/async_read.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/test/async_stream_helper.h"

using namespace mjlib::micro;
namespace base = mjlib::base;

using mjlib::micro::test::DutStream;

namespace {
struct Fixture {
  DutStream test_stream;

  char backing_buffer[64] = {};
  AsyncStreamBuf streambuf{backing_buffer, 0};

  AsyncReadUntilContext dut_context;
  char result_buffer[32] = {};

  Fixture() {
    dut_context.stream = &test_stream;
    dut_context.streambuf = &streambuf;
    dut_context.result = result_buffer;
    dut_context.delimiters = "\n";
  }

  void callback(error_code ec, size_t size) {
    auto cbk = test_stream.read_cbk_;
    test_stream.read_cbk_ = {};
    test_stream.read_data_ = {};
    cbk(ec, size);
  }
};
}

BOOST_FIXTURE_TEST_CASE(ReadUntilBasic, Fixture) {
  const std::string response("testline\n");

  int callback_count = 0;
  auto dut_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);

    BOOST_TEST(size == response.size());

    callback_count++;
  };

  dut_context.callback = dut_callback;
  AsyncReadUntil(dut_context);

  BOOST_TEST(!!test_stream.read_cbk_);
  BOOST_TEST(test_stream.read_data_.size() > 32);
  BOOST_TEST(test_stream.read_count_ == 1);

  // Basic operation is it ends up reading exactly one line.
  std::memcpy(test_stream.read_data_.data(), response.data(), response.size());
  BOOST_TEST(callback_count == 0);
  callback({}, response.size());

  BOOST_TEST(callback_count == 1);
  BOOST_TEST(std::string(result_buffer, response.size()) == response);

  // No more calls to read were made.
  BOOST_TEST(!test_stream.read_cbk_);
  BOOST_TEST(test_stream.read_count_ == 1);
}

BOOST_FIXTURE_TEST_CASE(ReadUntilStaggered, Fixture) {
  int callback_count = 0;

  auto dut_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);
    BOOST_TEST(size == 9);
    callback_count++;
  };
  dut_context.callback = dut_callback;

  AsyncReadUntil(dut_context);

  BOOST_TEST(!!test_stream.read_cbk_);
  BOOST_TEST(test_stream.read_data_.size() > 32);
  BOOST_TEST(test_stream.read_count_ == 1);

  {
    std::string fragment1 = "testl";
    std::memcpy(test_stream.read_data_.data(),
                fragment1.data(), fragment1.size());
    BOOST_TEST(callback_count == 0);
    callback({}, fragment1.size());
  }

  BOOST_TEST(callback_count == 0);

  // We did get another call to read.
  BOOST_TEST(!!test_stream.read_cbk_);
  BOOST_TEST(test_stream.read_count_ == 2);

  {
    std::string fragment2 = "ine\nmore";
    std::memcpy(test_stream.read_data_.data(),
                fragment2.data(), fragment2.size());
    callback({}, fragment2.size());
  }

  BOOST_TEST(callback_count == 1);
  // We did not get another call to read.
  BOOST_TEST(!test_stream.read_cbk_);

  const std::string line1 = "testline\n";
  BOOST_TEST(std::string(result_buffer, line1.size()) == line1);

  AsyncReadUntil(dut_context);

  BOOST_TEST(!!test_stream.read_cbk_);
  BOOST_TEST(test_stream.read_data_.size() > 32);

  // Now send another fragment which contains 2 full lines and extra.
  {
    std::string fragment3 = "line\nyetagain\nhi";
    std::memcpy(test_stream.read_data_.data(),
                fragment3.data(), fragment3.size());
    callback({}, fragment3.size());
  }

  // Our callback should have been invoked one more time.
  BOOST_TEST(callback_count == 2);
  // We did not get another call to read.
  BOOST_TEST(!test_stream.read_cbk_);

  const std::string line2 = "moreline\n";
  BOOST_TEST(std::string(result_buffer, line2.size()) == line2);

  // If we read again, we should get one more line without having any
  // further read calls being made.

  BOOST_TEST(test_stream.read_count_ == 3);
  AsyncReadUntil(dut_context);

  BOOST_TEST(callback_count == 3);
  BOOST_TEST(test_stream.read_count_ == 3);

  const std::string line3 = "yetagain\n";
  BOOST_TEST(std::string(result_buffer, line3.size()) == line3);
}

BOOST_FIXTURE_TEST_CASE(ReadUntilOverflow, Fixture) {
  // Verify a reasonable thing happens when our streambuf fills up
  // before we get a delimeter.
  int callback_count = 0;
  auto dut_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST((ec == errc::kDelimiterNotFound));
    BOOST_TEST(size == 64);
    callback_count++;
  };
  dut_context.callback = dut_callback;

  AsyncReadUntil(dut_context);

  for (int i = 0; i < 8; i++) {
    BOOST_TEST(callback_count == 0);

    BOOST_TEST(!!test_stream.read_cbk_);
    BOOST_TEST(test_stream.read_data_.size() > 0);

    std::string fragment = "abcdefghi";
    const auto to_read = std::min<size_t>(
        fragment.size(), test_stream.read_data_.size());
    std::memcpy(test_stream.read_data_.data(),
                fragment.data(), to_read);
    callback({}, to_read);
  }

  // Our final call should have overfilled our streambuf.
  BOOST_TEST(callback_count == 1);

  // At this point, we should be able to read again, and get an actual
  // line.
  auto complete_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);
    BOOST_TEST(size == 6);
    callback_count++;
  };

  dut_context.callback = complete_callback;
  AsyncReadUntil(dut_context);

  const std::string finalline = "onemo\n";
  {
    BOOST_TEST(!!test_stream.read_cbk_);
    BOOST_TEST(test_stream.read_data_.size() > 32);
    std::memcpy(test_stream.read_data_.data(),
                finalline.data(), finalline.size());
    callback({}, finalline.size());
  }

  BOOST_TEST(callback_count == 2);
  BOOST_TEST(std::string(result_buffer, finalline.size()) == finalline);
}

BOOST_FIXTURE_TEST_CASE(IgnoreUntilBasic, Fixture) {
  // Start the streambuf with some content.
  std::string initial_content("start");
  std::memcpy(backing_buffer, initial_content.data(), initial_content.size());
  streambuf.size = initial_content.size();

  int callback_count = 0;
  auto dut_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);
    // We don't care what the size is.
    callback_count++;
  };

  dut_context.callback = dut_callback;

  AsyncIgnoreUntil(dut_context);

  BOOST_TEST(!!test_stream.read_cbk_);
  {
    const std::string fragment = "stuff\nanother line\n";
    std::memcpy(test_stream.read_data_.data(),
                fragment.data(),
                fragment.size());
    callback({}, fragment.size());
  }

  BOOST_TEST(callback_count == 1);

  // If we follow up with a ReadUntil, we get the following line with
  // no further reads.
  const std::string another_line = "another line\n";
  auto another_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);
    BOOST_TEST(size = another_line.size());
    callback_count++;
  };

  dut_context.callback = another_callback;

  AsyncReadUntil(dut_context);
  BOOST_TEST(callback_count == 2);
  BOOST_TEST(std::string(result_buffer, another_line.size()) == another_line);
}

BOOST_FIXTURE_TEST_CASE(ReadUntilLineLongerThanResultIsCapped, Fixture) {
  // The fixture deliberately uses streambuf.size() = 64 and
  // result.size() = 32. Previously, a line longer than the result
  // span (but no longer than streambuf) ran an unbounded memcpy past
  // the end of result_buffer; now the copy is capped at
  // result.size() and the callback reports that capped size.
  static_assert(sizeof(result_buffer) == 32);
  // 49 'X' + delimiter '\n', total 50 bytes -- comfortably exceeds
  // 32 and fits in the 64-byte streambuf.
  const std::string response(49, 'X');
  const std::string line = response + "\n";

  // Sentinel just past result_buffer; pre-fix this would have been
  // overwritten by the memcpy.
  char canary = 0x55;
  (void)&canary;  // address-taken so the compiler can't merge it away

  int callback_count = 0;
  size_t reported_size = 0;
  auto dut_callback = [&](error_code ec, std::size_t size) {
    BOOST_TEST(!ec);
    reported_size = size;
    ++callback_count;
  };
  dut_context.callback = dut_callback;

  AsyncReadUntil(dut_context);

  BOOST_TEST(test_stream.read_data_.size() > 32);
  std::memcpy(test_stream.read_data_.data(), line.data(), line.size());
  callback({}, line.size());

  BOOST_TEST(callback_count == 1);
  BOOST_TEST(reported_size == sizeof(result_buffer));
  BOOST_TEST(canary == 0x55);
  BOOST_TEST(std::string(result_buffer, sizeof(result_buffer)) ==
             std::string(sizeof(result_buffer), 'X'));
}
