// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/program_options_archive.h"

#include <sstream>

#include <boost/test/auto_unit_test.hpp>

namespace {
struct MyStruct {
  int int_value = 1;
  std::string string_value = "2";
  std::array<int, 3> array_value = {{ 4, 5, 6}};

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(int_value));
    a->Visit(MJ_NVP(string_value));
    a->Visit(MJ_NVP(array_value));
  }
};

std::string StripSpaces(const std::string& in) {
  std::stringstream result;
  for (char c : in) {
    if (c != ' ') { result.write(&c, 1); }
  }
  return result.str();
}
}

BOOST_AUTO_TEST_CASE(ProgramOptionsArchiveTest) {
  MyStruct my_struct;
  boost::program_options::options_description desc;
  mjlib::base::ProgramOptionsArchive(&desc).Accept(&my_struct);

  std::stringstream ss;
  ss << desc;
  std::string no_spaces = StripSpaces(ss.str());
  BOOST_TEST(no_spaces == "--int_value\n--string_value\n--array_value.0\n--array_value.1\n--array_value.2\n");
}
