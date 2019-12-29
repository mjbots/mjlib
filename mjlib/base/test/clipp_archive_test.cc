// Copyright 2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/base/clipp_archive.h"

#include <cctype>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/args.h"
#include "mjlib/base/args_visitor.h"

namespace {
enum MyEnum {
  kValue1 = 1,
  kValue2 = 2,
};

std::map<MyEnum, const char*> MyEnumMapper() {
  return {
    { kValue1, "kValue1", },
    { kValue2, "kValue2", },
  };
}

struct MyStruct {
  int int_value = 1;
  std::string string_value = "2";
  MyEnum enum_value = kValue1;

  // TODO
  //  array_value
  //  substruct_value

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVPT(int_value).help("special text"));
    a->Visit(MJ_NVPT(string_value).label("STR"));
    a->Visit(MJ_ENUMT(enum_value, MyEnumMapper).help("stuff"));
  }
};

std::string CollpaseWhitespace(const std::string& str) {
  std::ostringstream ostr;
  bool was_whitespace = true;
  for (char c : str) {
    if (!was_whitespace || !std::isspace(c)) {
      ostr.put(c);
    }
    was_whitespace = std::isspace(c);
  }
  return ostr.str();
}
}

BOOST_AUTO_TEST_CASE(ClippArchiveTest) {
  MyStruct my_struct;
  auto group = mjlib::base::ClippArchive().Accept(&my_struct).release();

  const auto actual_usage = CollpaseWhitespace(
      boost::lexical_cast<std::string>(clipp::usage_lines(group)) +
      boost::lexical_cast<std::string>(clipp::documentation(group)));
  BOOST_TEST(actual_usage == R"XX([--int_value <arg>] [--string_value <STR>] [--enum_value <arg>] --int_value <arg>
special text
--string_value <STR>
--enum_value <arg>
stuff (kValue1/kValue2))XX");

  mjlib::base::Args args(
      {"progname",
            "--int_value", "87",
            "--string_value", "foo",
            });
  clipp::parse(args.argc, args.argv, group);
  BOOST_TEST(my_struct.int_value == 87);
  BOOST_TEST(my_struct.string_value == "foo");
}
