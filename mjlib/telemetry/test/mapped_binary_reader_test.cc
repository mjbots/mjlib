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

#include "mjlib/telemetry/mapped_binary_reader.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/test/all_types_struct.h"
#include "mjlib/telemetry/binary_schema_parser.h"

using namespace mjlib;
namespace tl = telemetry;

BOOST_AUTO_TEST_CASE(SameTypes) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<base::test::AllTypesTest>());
  tl::MappedBinaryReader<base::test::AllTypesTest> dut{&parser};
  auto same = dut.Read(
      tl::BinaryWriteArchive::Write(base::test::AllTypesTest()));
}

namespace {
struct OnlySome {
  base::test::SubTest1 value_object{10};
  int8_t value_i8 = 34;  // in a different order
  int32_t extra = 27;    // not present in AllTypesTest

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value_object));
    a->Visit(MJ_NVP(value_i8));
    a->Visit(MJ_NVP(extra));
  }
};
}

BOOST_AUTO_TEST_CASE(SchemaEvolution) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<base::test::AllTypesTest>());
  tl::MappedBinaryReader<OnlySome> dut{&parser};
  auto only_some = dut.Read(
      tl::BinaryWriteArchive::Write(base::test::AllTypesTest()));
  BOOST_TEST(only_some.value_i8 == -1);
  BOOST_TEST(only_some.value_object.value_u32 == 3);
  BOOST_TEST(only_some.extra == 27);
}

namespace {
struct TypeError {
  int32_t value_i8 = 103;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value_i8));
  }
};
}

BOOST_AUTO_TEST_CASE(ChangeTypeError) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<base::test::AllTypesTest>());

  auto is_type_error = [](const base::system_error& error) {
    return error.code() == telemetry::errc::kTypeMismatch;
  };
  // We use a lambda just to avoid macro problems with templates.
  auto create = [&]() {
    tl::MappedBinaryReader<TypeError> dut{&parser};
  };
  BOOST_CHECK_EXCEPTION(create(), base::system_error, is_type_error);
}
