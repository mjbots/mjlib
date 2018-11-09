// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include "async_exclusive.h"

#include "mbed.h"
#include "utest/utest.h"
#include "unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

void test_basic_async_exclusive() {
  int value = 0;
  AsyncExclusive<int> dut{&value};
}

utest::v1::status_t test_setup(const size_t number_of_cases) {
  GREENTEA_SETUP(40, "default_auto");
  return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
  Case("basic_async_exclusive", test_basic_async_exclusive),
};

Specification specification(test_setup, cases);

int main() {
  return !Harness::run(specification);
}
