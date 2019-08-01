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

#include "mjlib/base/visitor.h"

#include <boost/test/auto_unit_test.hpp>

namespace {
struct NonNativeStruct {
  int a = 0;
  double b = 1;
};

struct Archive {
  template <typename NameValuePair>
  void Visit(const NameValuePair& nvp) {
    names.push_back(nvp.name());
  }

  std::vector<std::string> names;
};
}

namespace mjlib {
namespace base {
template <>
struct ExternalSerializer<NonNativeStruct> {
  template <typename Archive>
  void Serialize(NonNativeStruct* o, Archive* a) {
    a->Visit(mjlib::base::MakeNameValuePair(&o->a, "a"));
    a->Visit(mjlib::base::MakeNameValuePair(&o->b, "b"));
  }
};
}
}

BOOST_AUTO_TEST_CASE(ExternalSerializeTest) {
  NonNativeStruct dut;
  Archive archive;

  mjlib::base::Serialize(&dut, &archive);

  std::vector<std::string> expected = {
    "a",
    "b",
  };
  BOOST_TEST(archive.names == expected,
             boost::test_tools::per_element());
}

namespace {
template <typename T>
struct NonNativeTemplate {
  T c = {};
  T d = {};
};
}

namespace mjlib {
namespace base {
template <typename T>
struct ExternalSerializer<NonNativeTemplate<T>> {
  template <typename Archive>
  void Serialize(NonNativeTemplate<T>* o, Archive* a) {
    a->Visit(mjlib::base::MakeNameValuePair(&o->c, "c"));
    a->Visit(mjlib::base::MakeNameValuePair(&o->d, "d"));
  }
};
}
}

BOOST_AUTO_TEST_CASE(ExternalTemplateSerialize) {
  NonNativeTemplate<int> dut;
  Archive archive;

  mjlib::base::Serialize(&dut, &archive);

  std::vector<std::string> expected = {
    "c",
    "d",
  };
  BOOST_TEST(archive.names == expected,
             boost::test_tools::per_element());
}
