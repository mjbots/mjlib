// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/stream_pipe.h"

#include <cstddef>
#include <deque>

#include <boost/test/auto_unit_test.hpp>

#include <fmt/format.h>

#include "mjlib/micro/event_queue.h"

using namespace mjlib::micro;
namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(BasicStreamPipe) {
  EventQueue event_queue;
  StreamPipe dut(event_queue.MakePoster());

  const char data_to_send[] = "stuff to send";
  char data_to_receive[4] = {};

  int write_complete = 0;
  std::ptrdiff_t write_size = 0;

  dut.side_a()->AsyncWriteSome(
      data_to_send,
      [&](error_code ec, std::ptrdiff_t size) {
        BOOST_TEST(!ec);
        write_complete++;
        write_size = size;
      });

  BOOST_TEST(write_complete == 0);
  BOOST_TEST(event_queue.empty());

  int read_complete = 0;
  std::ptrdiff_t read_size = 0;

  dut.side_b()->AsyncReadSome(
      base::string_span(data_to_receive, 4),
      [&](error_code ec, std::ptrdiff_t size) {
        BOOST_TEST(!ec);
        read_complete++;
        read_size = size;
      });

  BOOST_TEST(write_complete == 0);
  BOOST_TEST(read_complete == 0);
  BOOST_TEST(!event_queue.empty());

  event_queue.Poll();

  BOOST_TEST(write_complete == 1);
  BOOST_TEST(read_complete == 1);
  BOOST_TEST(std::string_view(data_to_receive, 4) == "stuf");
}

namespace {
class Writer {
 public:
  Writer(AsyncWriteStream* stream) : stream_(stream) {
    StartWrite(0);
  }

  int count() const { return count_; }

 private:
  void StartWrite(std::ptrdiff_t size) {
    count_++;
    fmt::format_to_n(buf_, sizeof(buf_) - 1, "{} bytes\n", size);

    stream_->AsyncWriteSome(
        buf_,
        [this](error_code ec, std::ptrdiff_t size) {
          BOOST_TEST(!ec);
          this->StartWrite(size);
        });
  }

  AsyncWriteStream* const stream_;
  char buf_[50] = {};
  int count_ = 0;
};
}

BOOST_AUTO_TEST_CASE(StreamPipeWriteEnqueue) {
  // Start a new write from within a write callback.
  EventQueue event_queue;
  StreamPipe dut(event_queue.MakePoster());
  Writer writer(dut.side_a());

  BOOST_TEST(writer.count() == 1);

  event_queue.Poll();
  BOOST_TEST(writer.count() == 1);

  // The writer always has something outstanding, and each incremental
  // write starts at the beginning of a packet.
  char to_receive[30] = {};
  int receive_count = 0;
  dut.side_b()->AsyncReadSome(
      base::string_span(to_receive, sizeof(to_receive)),
      [&](error_code ec, std::ptrdiff_t size) {
        BOOST_TEST(!ec);
        BOOST_TEST(size == 8);
        BOOST_TEST(std::string(to_receive, size) == "0 bytes\n");
        receive_count++;
      });

  BOOST_TEST(receive_count == 0);
  event_queue.Poll();
  BOOST_TEST(receive_count == 1);

  dut.side_b()->AsyncReadSome(
      base::string_span(to_receive, 1),
      [&](error_code ec, std::ptrdiff_t size) {
        BOOST_TEST(!ec);
        BOOST_TEST(size == 1);
        BOOST_TEST(std::string(to_receive, size) == "8");
        receive_count++;
      });

  BOOST_TEST(receive_count == 1);
  event_queue.Poll();
  BOOST_TEST(receive_count == 2);

  // Finally, read another slightly larger piece.
  dut.side_b()->AsyncReadSome(
      base::string_span(to_receive, 3),
      [&](error_code ec, std::ptrdiff_t size) {
        BOOST_TEST(!ec);
        BOOST_TEST(size == 3);
        BOOST_TEST(std::string(to_receive, size) == "1 b");
        receive_count++;
      });

  BOOST_TEST(receive_count == 2);
  event_queue.Poll();
  BOOST_TEST(receive_count == 3);
}
