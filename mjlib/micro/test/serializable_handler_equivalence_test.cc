// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

/// @file
///
/// Verify that the compact SerializableHandler produces results
/// identical to the original fully templated implementation, for
/// every operation, with both field tables and field functions.

#include "mjlib/micro/serializable_handler.h"
#include "mjlib/micro/test/serializable_handler_equivalence.h"

#include <boost/test/auto_unit_test.hpp>

#include <cstring>
#include <string>

namespace equivalence_test {

enum Plain {
  kPlainA,
  kPlainB,
  kPlainC,
};

enum class Signed : int8_t {
  kNeg = -3,
  kZero = 0,
  kPos = 100,
};

enum class Wide : uint32_t {
  kSmall = 1,
  kBig = 0xfffffff0u,
};

}

using namespace equivalence_test;

namespace mjlib {
namespace base {

template <>
struct IsEnum<Plain> {
  static constexpr bool value = true;
  static std::array<std::pair<Plain, const char*>, 3> map() {
    return {{
        { kPlainA, "a" },
        { kPlainB, "b" },
        { kPlainC, "c" },
      }};
  }
};

template <>
struct IsEnum<Signed> {
  static constexpr bool value = true;
  static constexpr std::array<std::pair<Signed, const char*>, 3> map() {
    return {{
        { Signed::kNeg, "neg" },
        { Signed::kZero, "zero" },
        { Signed::kPos, "pos" },
      }};
  }
};

template <>
struct IsEnum<Wide> {
  static constexpr bool value = true;
  static constexpr std::array<std::pair<Wide, const char*>, 2> map() {
    return {{
        { Wide::kSmall, "small" },
        { Wide::kBig, "big" },
      }};
  }
};

}
}

namespace equivalence_test {

struct Empty {
  template <typename Archive>
  void Serialize(Archive*) {
  }
};

struct Inner {
  int32_t value = 23;
  float gain = 1.5f;
  Plain mode = kPlainB;
  std::array<int16_t, 2> pair = {{-4, 5}};

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value));
    a->Visit(MJ_NVP(gain));
    a->Visit(MJ_NVP(mode));
    a->Visit(MJ_NVP(pair));
  }
};

struct Middle {
  Inner inner;
  std::array<Inner, 2> inners;
  Empty empty;
  bool flag = true;

  Middle() {
    inners[1].value = 99;
    inners[1].mode = kPlainC;
  }

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(inner));
    a->Visit(MJ_NVP(inners));
    a->Visit(MJ_NVP(empty));
    a->Visit(MJ_NVP(flag));
  }
};

// A type which is not natively serializable, and a wrapper which
// makes it so, as mjlib/base/eigen.h does for Eigen types.
struct Vec3 {
  float x = 1.0f;
  float y = 2.0f;
  float z = 3.0f;
};

struct Opaque {
  int16_t a = 7;
  uint8_t b = 200;
};

struct OpaqueWrapper {
  explicit OpaqueWrapper(Opaque* o) : o_(o) {}

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(mjlib::base::MakeNameValuePair(&o_->a, "a"));
    a->Visit(mjlib::base::MakeNameValuePair(&o_->b, "b"));
  }

  Opaque* o_;
};

}

namespace mjlib {
namespace base {

template <>
struct ExternalSerializer<Vec3> {
  template <typename PairReceiver>
  void Serialize(Vec3* v, PairReceiver receiver) {
    auto& data = *reinterpret_cast<std::array<float, 3>*>(&v->x);
    receiver(MJ_NVP(data));
  }
};

template <>
struct ExternalSerializer<Opaque> {
  template <typename PairReceiver>
  void Serialize(Opaque* o, PairReceiver receiver) {
    OpaqueWrapper wrapper(o);
    receiver(MakeNameValuePair(&wrapper, ""));
  }
};

}
}

namespace equivalence_test {

struct Everything {
  bool b = true;
  int8_t i8 = -8;
  int16_t i16 = -1600;
  int32_t i32 = -320000;
  int64_t i64 = std::numeric_limits<int64_t>::min();
  uint8_t u8 = 250;
  uint16_t u16 = 65000;
  uint32_t u32 = 4000000000u;
  uint64_t u64 = std::numeric_limits<uint64_t>::max();
  float f32 = -1.42e-6f;
  double f64 = 3.5e-9;
  Plain plain = kPlainC;
  Signed sgn = Signed::kNeg;
  Wide wide = Wide::kBig;
  std::array<float, 3> floats = {{6.0f, 7.0f, 8.0f}};
  std::array<Signed, 2> enums = {{Signed::kPos, Signed::kNeg}};
  std::array<std::array<uint8_t, 2>, 2> nested_arrays = {{{{1, 2}}, {{3, 4}}}};
  std::array<std::optional<int32_t>, 2> optionals = {{std::nullopt, 12}};
  std::optional<int32_t> opt_unset;
  std::optional<int64_t> opt_set = -44;
  std::optional<float> opt_float = 2.5f;
  std::optional<double> opt_double;
  Middle middle;
  Vec3 vec;
  Opaque opaque;
  std::array<Vec3, 2> vecs;
  float last = 0.25f;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(b));
    a->Visit(MJ_NVP(i8));
    a->Visit(MJ_NVP(i16));
    a->Visit(MJ_NVP(i32));
    a->Visit(MJ_NVP(i64));
    a->Visit(MJ_NVP(u8));
    a->Visit(MJ_NVP(u16));
    a->Visit(MJ_NVP(u32));
    a->Visit(MJ_NVP(u64));
    a->Visit(MJ_NVP(f32));
    a->Visit(MJ_NVP(f64));
    a->Visit(MJ_NVP(plain));
    a->Visit(MJ_NVP(sgn));
    a->Visit(MJ_NVP(wide));
    a->Visit(MJ_NVP(floats));
    a->Visit(MJ_NVP(enums));
    a->Visit(MJ_NVP(nested_arrays));
    a->Visit(MJ_NVP(optionals));
    a->Visit(MJ_NVP(opt_unset));
    a->Visit(MJ_NVP(opt_set));
    a->Visit(MJ_NVP(opt_float));
    a->Visit(MJ_NVP(opt_double));
    a->Visit(MJ_NVP(middle));
    a->Visit(MJ_NVP(vec));
    a->Visit(MJ_NVP(opaque));
    a->Visit(MJ_NVP(vecs));
    // A field with a name that differs from the member.
    a->Visit(mjlib::base::MakeNameValuePair(&last, "renamed_last"));
  }
};

Everything MakeModified() {
  Everything r;
  r.b = false;
  r.i8 = 127;
  r.i16 = 3;
  r.i32 = std::numeric_limits<int32_t>::min();
  r.i64 = 123456789012345ll;
  r.u8 = 0;
  r.u16 = 1;
  r.u32 = 2;
  r.u64 = 3;
  r.f32 = 1e30f;
  r.f64 = -2.0;
  r.plain = kPlainA;
  r.sgn = Signed::kPos;
  r.wide = Wide::kSmall;
  r.floats[1] = -0.0f;
  r.optionals[0] = -1;
  r.optionals[1].reset();
  r.opt_unset = 5;
  r.opt_set.reset();
  r.opt_double = 1e100;
  r.middle.inner.gain = 99.5f;
  r.middle.inners[0].pair[1] = 32767;
  r.middle.flag = false;
  r.vec.z = -9.0f;
  r.opaque.a = -1;
  r.vecs[1].y = 123.0f;
  r.last = 1e-40f;
  return r;
}

template <typename T>
void CheckAll(const T& initial) {
  mjlib::micro::test::EquivalenceOptions options;
  // The legacy implementation parsed all optionals with strtoull,
  // which truncated floating point values.
  options.skip_set = [](std::string_view key, std::string_view) {
    return key.substr(0, 3) == "opt";
  };
  mjlib::micro::test::CheckEquivalence<T>(initial, options);
}

}

BOOST_AUTO_TEST_CASE(SerializableHandlerEnumDescriptors) {
  using mjlib::micro::EnumDescriptor;
  using mjlib::micro::GetTypeDescriptor;
  auto enum_desc = [](const mjlib::micro::TypeDescriptor* desc) {
    return static_cast<const EnumDescriptor*>(desc);
  };
  // A constexpr IsEnum map results in a constant table.
  BOOST_TEST(enum_desc(GetTypeDescriptor<Signed>())->entries != nullptr);
  BOOST_TEST(enum_desc(GetTypeDescriptor<Wide>())->entries != nullptr);
  BOOST_TEST(enum_desc(GetTypeDescriptor<Plain>())->entries == nullptr);
  BOOST_TEST(enum_desc(GetTypeDescriptor<Plain>())->visit_entries != nullptr);
}

BOOST_AUTO_TEST_CASE(SerializableHandlerEquivalence) {
  CheckAll(Everything());
  CheckAll(MakeModified());
  CheckAll(Middle());
  CheckAll(Inner());
  CheckAll(Empty());
}

BOOST_AUTO_TEST_CASE(SerializableHandlerEquivalenceExternalRoot) {
  CheckAll(Opaque());
}

BOOST_AUTO_TEST_CASE(SerializableHandlerSetOptional) {
  Everything dut;
  mjlib::micro::SerializableHandler<Everything> handler(&dut);
  BOOST_TEST(handler.Set("opt_float", "-1.25") == 0);
  BOOST_TEST(*dut.opt_float == -1.25f);
  BOOST_TEST(handler.Set("opt_double", "3.5") == 0);
  BOOST_TEST(*dut.opt_double == 3.5);
  BOOST_TEST(handler.Set("opt_unset", "-7") == 0);
  BOOST_TEST(*dut.opt_unset == -7);
  BOOST_TEST(handler.Set("optionals.0", "0x20") == 0);
  BOOST_TEST(*dut.optionals[0] == 32);
  BOOST_TEST(handler.Set("optionals.2", "1") == 1);
}


