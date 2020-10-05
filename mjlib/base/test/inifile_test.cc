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

#include "mjlib/base/inifile.h"

#include <string>
#include <vector>

#include <boost/test/auto_unit_test.hpp>

BOOST_AUTO_TEST_CASE(BasicTest) {
  std::istringstream input(R"XXX(
nocontext=2

# comment
[context1]
stuff = true
nested.stuff = hello_there

[context2.deep]
yo_there = foo # trailing comment
)XXX");
  const auto result = mjlib::base::ReadIniOptionStream(input);

  std::vector<std::string> expected = {
    "--nocontext", "2",
    "--context1.stuff", "true",
    "--context1.nested.stuff", "hello_there",
    "--context2.deep.yo_there", "foo",
  };

  BOOST_TEST(result == expected, boost::test_tools::per_element());
}
