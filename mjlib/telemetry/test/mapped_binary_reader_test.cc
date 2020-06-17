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

BOOST_AUTO_TEST_CASE(InverseSchemaEvolution) {
  tl::BinarySchemaParser parser(tl::BinarySchemaArchive::Write<OnlySome>());
  tl::MappedBinaryReader<base::test::AllTypesTest> dut{&parser};
  auto all = dut.Read(tl::BinaryWriteArchive::Write(OnlySome()));
  BOOST_TEST(all.value_i8 == 34);
  BOOST_TEST(all.value_object.value_u32 == 10);
}

BOOST_AUTO_TEST_CASE(ArrayEvolution) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<std::vector<OnlySome>>());
  tl::MappedBinaryReader<std::vector<base::test::AllTypesTest>> dut{&parser};
  auto all = dut.Read(tl::BinaryWriteArchive::Write(
                          std::vector<OnlySome>{{}, {}}));
  BOOST_TEST(all.size() == 2);
  BOOST_TEST(all[0].value_i8 == 34);
  BOOST_TEST(all[1].value_i8 == 34);
}

BOOST_AUTO_TEST_CASE(FixedArrayEvolution) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<std::array<OnlySome, 2>>());
  tl::MappedBinaryReader<std::array<base::test::AllTypesTest, 2>> dut{&parser};
  auto all = dut.Read(tl::BinaryWriteArchive::Write(
                          std::array<OnlySome, 2>{}));
  BOOST_TEST(all.size() == 2);
  BOOST_TEST(all[0].value_i8 == 34);
  BOOST_TEST(all[1].value_i8 == 34);
}

BOOST_AUTO_TEST_CASE(OptionalEvolution) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<std::optional<OnlySome>>());
  tl::MappedBinaryReader<std::optional<base::test::AllTypesTest>> dut{&parser};
  auto all = dut.Read(tl::BinaryWriteArchive::Write(
                          std::optional<OnlySome>{OnlySome()}));
  BOOST_TEST(!!all);
  BOOST_TEST(all->value_i8 == 34);
}

namespace {
enum class NewEnumeration : int {
  kValue1,
  kNextValue = 5,
  kAnotherValue = 20,
  kYetMoreValues = 50,
};
}

namespace mjlib {
namespace base {

template <>
struct IsEnum<NewEnumeration> {
  static constexpr bool value = true;

  static std::map<NewEnumeration, const char*> map() {
    using NE = NewEnumeration;
    return {
      { NE::kValue1, "kValue1" },
      { NE::kNextValue, "kNextValue" },
      { NE::kAnotherValue, "kAnotherValue" },
      { NE::kYetMoreValues, "kYetMoreValues" },
    };
  }
};

}
}

BOOST_AUTO_TEST_CASE(EnumEvolution) {
  tl::BinarySchemaParser parser(
      tl::BinarySchemaArchive::Write<base::test::TestEnumeration>());
  tl::MappedBinaryReader<NewEnumeration> dut{&parser};
  auto all = dut.Read(tl::BinaryWriteArchive::Write(
                          base::test::TestEnumeration::kAnotherValue));
  BOOST_TEST((all == NewEnumeration::kAnotherValue));
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

namespace {
struct NeedsExternalSerialization {
  int32_t stuff = 10;
};

struct WrappedExternal {
  int32_t baz = 11;
};

struct Compatible {
  int32_t baz = 23;
  int32_t bing = 200;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(baz));
    a->Visit(MJ_NVP(bing));
  }
};

struct WrappedExternalWrapper {
  WrappedExternalWrapper(WrappedExternal* o) : o_(o) {}

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(mjlib::base::MakeNameValuePair(&o_->baz, "baz"));
  }

  WrappedExternal* o_;
};
}

namespace mjlib {
namespace base {

template <>
struct ExternalSerializer<NeedsExternalSerialization> {
  template <typename PairReceiver>
  void Serialize(NeedsExternalSerialization* o, PairReceiver receiver) {
    receiver(mjlib::base::MakeNameValuePair(&o->stuff, ""));
  }
};

template <>
struct ExternalSerializer<WrappedExternal> {
  template <typename PairReceiver>
  void Serialize(WrappedExternal* o, PairReceiver receiver) {
    WrappedExternalWrapper wrapper{o};
    receiver(mjlib::base::MakeNameValuePair(&wrapper, ""));
  }
};

}
}

BOOST_AUTO_TEST_CASE(ParseExternalSerializer) {
  {
    tl::BinarySchemaParser parser(
        tl::BinarySchemaArchive::Write<base::test::AllTypesTest>());
    tl::MappedBinaryReader<NeedsExternalSerialization> dut{&parser};
    auto copy = dut.Read(tl::BinaryWriteArchive::Write(
                             base::test::AllTypesTest()));
    BOOST_TEST(copy.stuff == 10);
  }

  {
    tl::BinarySchemaParser parser(
        tl::BinarySchemaArchive::Write<int32_t>());
    tl::MappedBinaryReader<NeedsExternalSerialization> dut{&parser};
    auto copy = dut.Read(tl::BinaryWriteArchive::Write(static_cast<int32_t>(23)));
    BOOST_TEST(copy.stuff == 23);
  }

  {
    tl::BinarySchemaParser parser(
        tl::BinarySchemaArchive::Write<int32_t>());
    auto is_type_mismatch = [](const base::system_error& error) {
      return error.code() == telemetry::errc::kTypeMismatch;
    };
    BOOST_CHECK_EXCEPTION(
        (tl::MappedBinaryReader<WrappedExternal>{&parser}),
        base::system_error,
        is_type_mismatch);
  }

  {
    tl::BinarySchemaParser parser(
        tl::BinarySchemaArchive::Write<Compatible>());
    tl::MappedBinaryReader<WrappedExternal> dut{&parser};
    auto copy = dut.Read(tl::BinaryWriteArchive::Write(Compatible()));
    BOOST_TEST(copy.baz == 23);
  }
}
