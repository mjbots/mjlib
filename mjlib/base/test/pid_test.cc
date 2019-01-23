// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/pid.h"

#include <boost/test/auto_unit_test.hpp>

using namespace mjlib::base;

namespace {
PID::Config MakeConfig() {
  PID::Config config;

  config.kp = 1.0f;
  config.ki = 2.0f;
  config.kd = 3.0f;
  config.ilimit = 10.0f;

  return config;
}
}

BOOST_AUTO_TEST_CASE(BasicPid) {
  auto config = MakeConfig();
  PID::State state;

  PID dut{&config, &state};

  BOOST_TEST(state.integral == 0.0f);

  const float result = dut.Apply(1.0f, 3.0f,
                                 2.0f, 5.0f,
                                 100.0f);
  BOOST_TEST(result == -11.04f);
  BOOST_TEST(state.error == -2.0f);
  BOOST_TEST(state.error_rate == -3.0f);
  BOOST_TEST(state.p == -2.0f);
  BOOST_TEST(state.d == -9.0f);
  BOOST_TEST(state.pd == -11.0f);
  BOOST_TEST(state.command == result);
}


BOOST_AUTO_TEST_CASE(PidDesiredRate) {
  auto config = MakeConfig();
  config.max_desired_rate = 50.0f;
  PID::State state;

  PID dut{&config, &state};

  BOOST_TEST(state.command == 0.0);

  {
    const float result = dut.Apply(1.0f, 1.5f,
                                   2.0f, 5.0f,
                                   100.0f);

    BOOST_TEST(state.desired == 1.5f);
    BOOST_TEST(result == -9.51f);
  }

  {
    const float result = dut.Apply(1.0f, 3.0f,
                                   2.0f, 5.0f,
                                   100.0f);
    BOOST_TEST(state.desired == 2.0f);
    BOOST_TEST(result == -10.03f);
  }
}
