// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/clipp.h"

#include <sstream>

#include <boost/test/auto_unit_test.hpp>

BOOST_AUTO_TEST_CASE(ClippIniParse) {
  std::istringstream istr(R"XX(
sample=1
other = hello
)XX");

  int sample = 0;
  std::string other = "";
  auto group = clipp::group(
      clipp::option("s", "sample") & clipp::value("", sample),
      clipp::option("o", "other") & clipp::value("", other)
                            );
  mjlib::base::ClippParseIni(istr, group);
  BOOST_TEST(sample == 1);
  BOOST_TEST(other == "hello");
}
