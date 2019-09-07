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

#include "mjlib/micro/async_stream.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::micro;
namespace base = mjlib::base;

namespace {
class DutStream : public AsyncStream {
 public:
  void AsyncReadSome(const base::string_span& data,
                     const SizeCallback& cbk) override {
    read_data_ = data;
    read_cbk_ = cbk;
    read_count_++;
  }

  void AsyncWriteSome(const std::string_view& data,
                      const SizeCallback& cbk) override {
    write_data_ = data;
    write_cbk_ = cbk;
    write_count_++;
  }

  base::string_span read_data_;
  SizeCallback read_cbk_;
  int read_count_ = 0;

  std::string_view write_data_;
  SizeCallback write_cbk_;
  int write_count_ = 0;
};
}

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
