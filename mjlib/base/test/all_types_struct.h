// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/base/bytes.h"

namespace mjlib {
namespace base {
namespace test {

enum class TestEnumeration : int {
  kValue1,
  kNextValue = 5,
  kAnotherValue = 20,
};

inline std::map<TestEnumeration, const char*> TestEnumerationMapper() {
  using TE = TestEnumeration;
  return {
    { TE::kValue1, "kValue1" },
    { TE::kNextValue, "kNextValue" },
    { TE::kAnotherValue, "kAnotherValue" },
  };
}

struct SubTest1 {
  uint32_t value_u32 = 3;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value_u32));
  }
};

struct AllTypesTest {
  bool value_bool = false;
  int8_t value_i8 = -1;
  int16_t value_i16 = -2;
  int32_t value_i32 = -3;
  int64_t value_i64 = -4;
  uint8_t value_u8 = 5;
  uint16_t value_u16 = 6;
  uint32_t value_u32 = 7;
  uint64_t value_u64 = 8;
  float value_f32 = 9.0;
  double value_f64 = 10.0;
  base::Bytes value_bytes = { { static_cast<uint8_t>(11), static_cast<uint8_t>(12),
                                static_cast<uint8_t>(13) } };
  std::string value_str = "de";
  SubTest1 value_object;
  TestEnumeration value_enum = TestEnumeration::kValue1;
  std::vector<SubTest1> value_array = {{}};
  // std::map<std::string, int16_t> value_map = {
  //   { "abc", 2 },
  //   { "def", 4 },
  // };
  std::optional<int32_t> value_optional = 21;
  // std::variant<bool, int32_t> value_union = false;
  boost::posix_time::ptime value_timestamp =
      base::ConvertEpochMicrosecondsToPtime(1000000);
  boost::posix_time::time_duration value_duration =
      base::ConvertMicrosecondsToDuration(500000);

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value_bool));
    a->Visit(MJ_NVP(value_i8));
    a->Visit(MJ_NVP(value_i16));
    a->Visit(MJ_NVP(value_i32));
    a->Visit(MJ_NVP(value_i64));
    a->Visit(MJ_NVP(value_u8));
    a->Visit(MJ_NVP(value_u16));
    a->Visit(MJ_NVP(value_u32));
    a->Visit(MJ_NVP(value_u64));
    a->Visit(MJ_NVP(value_f32));
    a->Visit(MJ_NVP(value_f64));
    a->Visit(MJ_NVP(value_bytes));
    a->Visit(MJ_NVP(value_str));
    a->Visit(MJ_NVP(value_object));
    a->Visit(MJ_ENUM(value_enum, TestEnumerationMapper));
    a->Visit(MJ_NVP(value_array));
    // a->Visit(MJ_NVP(value_map));
    a->Visit(MJ_NVP(value_optional));
    // a->Visit(MJ_NVP(value_union));
    a->Visit(MJ_NVP(value_timestamp));
    a->Visit(MJ_NVP(value_duration));
  }
};

}
}
}
