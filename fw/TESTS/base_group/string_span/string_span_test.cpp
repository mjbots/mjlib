// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include "string_span.h"
#include "string_view.h"

#include "mbed.h"
#include "utest/utest.h"
#include "unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

void test_basic_string_span() {
  {
    string_span empty;
    TEST_ASSERT_EQUAL(empty.data(), nullptr);
    TEST_ASSERT_EQUAL(empty.size(), 0);
    TEST_ASSERT_EQUAL(empty.length(), 0);
    TEST_ASSERT_EQUAL(empty.empty(), true);
  }

  {
    char data[] = "stuff";
    auto span = string_span::ensure_z(data);
    TEST_ASSERT_EQUAL(span.size(), 5);
    TEST_ASSERT_EQUAL(span.length(), 5);
    TEST_ASSERT_EQUAL(span.empty(), false);
    TEST_ASSERT_EQUAL(span[0], 's');
    TEST_ASSERT_EQUAL(span(0), 's');

    int count = 0;
    for (char c: span) {
      TEST_ASSERT_NOT_EQUAL(c, 0);
      count++;
    }
    TEST_ASSERT_EQUAL(count, 5);

    span[1] = 'd';
    TEST_ASSERT_EQUAL(data[1], 'd');
  }
}

void test_basic_string_view() {
  {
    string_view empty;
    TEST_ASSERT_EQUAL(empty.data(), nullptr);
    TEST_ASSERT_EQUAL(empty.size(), 0);
    TEST_ASSERT_EQUAL(empty.length(), 0);
    TEST_ASSERT_EQUAL(empty.empty(), true);
  }

  {
    const char data[] = "stuff";
    auto dut = string_view::ensure_z(data);
    TEST_ASSERT_EQUAL(dut.size(), 5);
    TEST_ASSERT_EQUAL(dut.length(), 5);
    TEST_ASSERT_EQUAL(dut.empty(), false);
    TEST_ASSERT_EQUAL(dut[0], 's');
    TEST_ASSERT_EQUAL(dut(0), 's');

    int count = 0;
    for (char c: dut) {
      TEST_ASSERT_NOT_EQUAL(c, 0);
      count++;
    }
    TEST_ASSERT_EQUAL(count, 5);
  }
}

utest::v1::status_t test_setup(const size_t number_of_cases) {
  GREENTEA_SETUP(40, "default_auto");
  return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
  Case("basic_string_span", test_basic_string_span),
  Case("basic_string_view", test_basic_string_view),
};

Specification specification(test_setup, cases);

int main() {
  return !Harness::run(specification);
}
