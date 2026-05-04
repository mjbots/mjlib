// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <map>
#include <sstream>

#include <boost/test/auto_unit_test.hpp>

namespace {
enum MyEnum {
  kAlpha = 0,
  kBeta = 1,
  kGamma = 2,
};
}

namespace mjlib {
namespace base {
template <>
struct IsEnum<MyEnum> {
  static constexpr bool value = true;

  static std::map<MyEnum, const char*> map() {
    return {
      { kAlpha, "kAlpha" },
      { kBeta, "kBeta" },
      { kGamma, "kGamma" },
    };
  }
};
}
}

namespace {
struct MyStruct {
  int int_value = 1;
  std::string string_value = "2";
  std::array<int, 3> array_value = {{ 4, 5, 6}};
  MyEnum enum_value = kAlpha;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(int_value));
    a->Visit(MJ_NVP(string_value));
    a->Visit(MJ_NVP(array_value));
    a->Visit(MJ_NVP(enum_value));
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
  BOOST_TEST(no_spaces == "--int_value\n--string_value\n--array_value.0\n--array_value.1\n--array_value.2\n--enum_value\n");
}

BOOST_AUTO_TEST_CASE(ProgramOptionsArchiveEnumParse) {
  // Previously, instantiating ProgramOptionsArchive over a struct
  // containing an IsEnum-registered field was a hard compile error
  // (VisitEnumeration arity mismatch + helper depending on a
  // nonexistent NameValuePair API). This test exercises the full
  // declare-parse-notify round trip.
  namespace po = boost::program_options;

  MyStruct my_struct;
  po::options_description desc;
  mjlib::base::ProgramOptionsArchive(&desc).Accept(&my_struct);

  const char* argv[] = {"prog", "--enum_value", "kBeta"};
  po::variables_map vm;
  po::store(po::parse_command_line(3, argv, desc), vm);
  po::notify(vm);

  BOOST_TEST(my_struct.enum_value == kBeta);
}
