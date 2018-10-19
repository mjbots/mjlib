// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include "async_stream.h"

#include "mbed.h"
#include "utest/utest.h"
#include "unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

namespace {
class DutStream : public AsyncStream {
 public:
  void AsyncReadSome(const string_span& data, const SizeCallback& cbk) override {
    read_data_ = data;
    read_cbk_ = cbk;
    read_count_++;
  }

  void AsyncWriteSome(const string_view& data, const SizeCallback& cbk) override {
    write_data_ = data;
    write_cbk_ = cbk;
    write_count_++;
  }

  string_span read_data_;
  SizeCallback read_cbk_;
  int read_count_ = 0;

  string_view write_data_;
  SizeCallback write_cbk_;
  int write_count_ = 0;
};
}

void test_basic_async_stream() {
  DutStream dut_stream;

  {
    TEST_ASSERT_EQUAL(dut_stream.write_data_.empty(), true);
    TEST_ASSERT_EQUAL(dut_stream.write_cbk_.valid(), false);

    int write_error = -1;
    AsyncWrite(dut_stream, string_view::ensure_z("test of sending"),
               [&](ErrorCode error) {
                 write_error = error;
               });

    TEST_ASSERT_EQUAL(dut_stream.write_data_.size(), 15);
    TEST_ASSERT_EQUAL(dut_stream.write_cbk_.valid(), true);
    TEST_ASSERT_EQUAL(dut_stream.write_count_, 1);
    TEST_ASSERT_EQUAL(write_error, -1);

    dut_stream.write_cbk_(0, 2);
    TEST_ASSERT_EQUAL(dut_stream.write_data_.size(), 13);
    TEST_ASSERT_EQUAL(dut_stream.write_count_, 2);
    TEST_ASSERT_EQUAL(write_error, -1);

    dut_stream.write_cbk_(0, 13);
    TEST_ASSERT_EQUAL(dut_stream.write_count_, 2);

    TEST_ASSERT_EQUAL(write_error, 0);
  }

  {
    TEST_ASSERT_EQUAL(dut_stream.read_data_.empty(), true);
    TEST_ASSERT_EQUAL(dut_stream.read_cbk_.valid(), false);

    char buffer_to_read_into[10] = "";

    int read_error = -1;
    AsyncRead(dut_stream, string_span(buffer_to_read_into, sizeof(buffer_to_read_into)),
               [&](ErrorCode error) {
                 read_error = error;
               });

    TEST_ASSERT_EQUAL(dut_stream.read_data_.size(), 10);
    TEST_ASSERT_EQUAL(dut_stream.read_cbk_.valid(), true);
    TEST_ASSERT_EQUAL(dut_stream.read_count_, 1);
    TEST_ASSERT_EQUAL(read_error, -1);

    dut_stream.read_data_[0] = 'h';
    dut_stream.read_data_[1] = 'i';
    dut_stream.read_cbk_(0, 2);
    TEST_ASSERT_EQUAL(dut_stream.read_data_.size(), 8);
    TEST_ASSERT_EQUAL(dut_stream.read_count_, 1);
    TEST_ASSERT_EQUAL(read_error, -1);

    dut_stream.read_data_[0] = ' ';
    dut_stream.read_data_[1] = '1';
    dut_stream.read_cbk_(0, 8);
    TEST_ASSERT_EQUAL(dut_stream.read_count_, 2);

    TEST_ASSERT_EQUAL(read_error, 0);
    TEST_ASSERT_EQUAL(std::strcmp(buffer_to_read_into, "hi 1"), 0);
  }
}

utest::v1::status_t test_setup(const size_t number_of_cases) {
  GREENTEA_SETUP(40, "default_auto");
  return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
  Case("basic_async_stream", test_basic_async_stream),
};

Specification specification(test_setup, cases);

int main() {
  return !Harness::run(specification);
}
