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

#include "mjlib/micro/async_stream.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/test/async_stream_helper.h"

using namespace mjlib::micro;
namespace base = mjlib::base;

using mjlib::micro::test::DutStream;

BOOST_AUTO_TEST_CASE(BasicAsyncStream) {
  DutStream dut_stream;

  {
    BOOST_TEST(dut_stream.write_data_.empty() == true);
    BOOST_TEST(!dut_stream.write_cbk_);

    error_code write_error;
    AsyncWrite(dut_stream, std::string_view("test of sending"),
               [&](error_code error) {
                 write_error = error;
               });

    BOOST_TEST(dut_stream.write_data_.size() == 15);
    BOOST_TEST(!!dut_stream.write_cbk_);
    BOOST_TEST(dut_stream.write_count_ == 1);
    BOOST_TEST(!write_error);

    dut_stream.write_cbk_({}, 2);
    BOOST_TEST(dut_stream.write_data_.size() == 13);
    BOOST_TEST(dut_stream.write_count_ == 2);
    BOOST_TEST(!write_error);

    dut_stream.write_cbk_({}, 13);
    BOOST_TEST(dut_stream.write_count_ == 2);

    BOOST_TEST(!write_error);
  }

  {
    BOOST_TEST(dut_stream.read_data_.empty() == true);
    BOOST_TEST(!dut_stream.read_cbk_);

    char buffer_to_read_into[10] = "";

    error_code read_error;
    AsyncRead(dut_stream,
              base::string_span(buffer_to_read_into,
                                sizeof(buffer_to_read_into)),
              [&](error_code error) {
                read_error = error;
              });

    BOOST_TEST(dut_stream.read_data_.size() == 10);
    BOOST_TEST(!!dut_stream.read_cbk_);
    BOOST_TEST(dut_stream.read_count_ == 1);
    BOOST_TEST(!read_error);

    dut_stream.read_data_[0] = 'h';
    dut_stream.read_data_[1] = 'i';
    dut_stream.read_cbk_({}, 2);
    BOOST_TEST(dut_stream.read_data_.size() == 8);
    BOOST_TEST(dut_stream.read_count_ == 2);
    BOOST_TEST(!read_error);

    dut_stream.read_data_[0] = ' ';
    dut_stream.read_data_[1] = '1';
    dut_stream.read_cbk_({}, 8);
    BOOST_TEST(dut_stream.read_count_ == 2);

    BOOST_TEST(!read_error);
    BOOST_TEST(std::strcmp(buffer_to_read_into, "hi 1") == 0);
  }
}

BOOST_AUTO_TEST_CASE(AsyncWriterReentrantWriteFromCallback) {
  // Previously, calling AsyncWriter::Write from inside the callback
  // of a previous Write would destroy and overwrite the storage of
  // the still-executing lambda (callback_ is an inplace_function whose
  // operator= reuses the same byte buffer in place). Any captured
  // value in the outer lambda read after the recursive Write call
  // returned would observe bytes from the inner lambda. The fix is
  // to move callback_ to a stack local before invoking it.
  DutStream dut_stream;
  AsyncWriter writer;

  // Choose a large lambda for the inner write so that, pre-fix, its
  // bytes would clearly clobber the outer lambda's smaller capture.
  bool inner_completed = false;
  bool outer_capture_intact = false;

  const uint64_t kSentinel = 0xDEADBEEFCAFEBABEull;

  auto outer = [&dut_stream, &writer, &inner_completed,
                &outer_capture_intact,
                magic = kSentinel](error_code, std::ptrdiff_t) mutable {
    writer.Write(
        dut_stream, std::string_view("b"),
        [arr = std::array<uint64_t, 5>{0xAAAA1111ull, 0xBBBB2222ull,
                                       0xCCCC3333ull, 0xDDDD4444ull,
                                       0xEEEE5555ull},
         &inner_completed](error_code, std::ptrdiff_t) {
          (void)arr;
          inner_completed = true;
        });

    // After the recursive Write, the outer lambda's `magic` capture
    // must still read back as kSentinel. Pre-fix it would contain
    // bytes of the inner lambda's `arr`.
    outer_capture_intact = (magic == kSentinel);
  };

  writer.Write(dut_stream, std::string_view("a"), outer);

  // Drive the outer write completion; this invokes outer().
  dut_stream.write_cbk_({}, 1);
  // Drive the inner write completion to keep the chain symmetric.
  if (dut_stream.write_cbk_) { dut_stream.write_cbk_({}, 1); }

  BOOST_TEST(outer_capture_intact);
  BOOST_TEST(inner_completed);
}
