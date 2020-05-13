// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include <boost/lexical_cast.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/args.h"
#include "mjlib/base/args_visitor.h"
#include "mjlib/base/collapse_whitespace.h"

namespace {
enum MyEnum {
  kValue1 = 1,
  kValue2 = 2,
};
}

namespace mjlib {
namespace base {
template <>
struct IsEnum<MyEnum> {
  static constexpr bool value = true;

  static std::map<MyEnum, const char*> map() {
    return {
      { kValue1, "kValue1", },
      { kValue2, "kValue2", },
          };
  }
};
}
}

namespace {

struct SubStruct {
  bool bool_switch = false;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(bool_switch));
  }
};

struct MyStruct {
  int int_value = 1;
  std::string string_value = "2";
  MyEnum enum_value = kValue1;
  SubStruct sub_struct;
  std::array<SubStruct, 2> array_subs = {};
  std::array<int, 3> array_scalars = {};

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVPT(int_value).help("special text"));
    a->Visit(MJ_NVPT(string_value).label("STR"));
    a->Visit(MJ_NVPT(enum_value).help("stuff"));
    a->Visit(MJ_NVP(sub_struct));
    a->Visit(MJ_NVP(array_subs));
    a->Visit(MJ_NVP(array_scalars));
  }
};
}

BOOST_AUTO_TEST_CASE(ClippArchiveTest) {
  MyStruct my_struct;
  auto group = mjlib::base::ClippArchive().Accept(&my_struct).release();

  const auto actual_usage = mjlib::base::CollapseWhitespace(
      boost::lexical_cast<std::string>(clipp::usage_lines(group)) +
      boost::lexical_cast<std::string>(clipp::documentation(group)));
  const std::string expected = R"XX([int_value <arg>] [string_value <STR>] [enum_value <arg>] [sub_struct.bool_switch <arg>]
[array_subs.0.bool_switch <arg>] [array_subs.1.bool_switch <arg>] [array_scalars.0 <arg>]
[array_scalars.1 <arg>] [array_scalars.2 <arg>] int_value <arg>
special text
string_value <STR>
enum_value <arg>
stuff (kValue1/kValue2)
sub_struct.bool_switch <arg>
array_subs.0.bool_switch <arg>
array_subs.1.bool_switch <arg>
array_scalars.0 <arg>
array_scalars.1 <arg>
array_scalars.2 <arg>
)XX";
  BOOST_TEST(actual_usage == expected);

  mjlib::base::Args args(
      {"progname",
            "int_value", "87",
            "string_value", "foo",
            });
  clipp::parse(args.argc, args.argv, group);
  BOOST_TEST(my_struct.int_value == 87);
  BOOST_TEST(my_struct.string_value == "foo");
}
