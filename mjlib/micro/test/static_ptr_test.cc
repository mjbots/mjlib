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

#include "mjlib/micro/static_ptr.h"

#include <boost/test/auto_unit_test.hpp>

namespace micro = mjlib::micro;

namespace {
struct Simple {
  int a = 0;
  bool b = 0;
  double c = 0.0;

  Simple(int a_in, bool b_in, double c_in)
      : a(a_in), b(b_in), c(c_in) {}
};

class NontrivialDestructor {
 public:
  NontrivialDestructor(int* destruct_count)
      : destruct_count_(destruct_count) {}

  ~NontrivialDestructor() {
    (*destruct_count_)++;
  }

 private:
  int* destruct_count_;
};
}

BOOST_AUTO_TEST_CASE(BasicStaticPtr) {
  {
    micro::StaticPtr<bool, 16> dut;
    BOOST_TEST((!dut));
    dut.reset();
    BOOST_TEST((!dut));
    BOOST_TEST(sizeof(dut) > sizeof(bool));
    BOOST_TEST(sizeof(dut) >= 16);
  }

  {
    micro::StaticPtr<bool, 16> dut(true);
    BOOST_TEST(!!dut);
    BOOST_TEST(*dut == true);
    dut = false;
    BOOST_TEST(*dut == false);

    micro::StaticPtr<bool, 16> other;
    BOOST_TEST(!other);

    std::swap(dut, other);
    BOOST_TEST(*other == false);
    BOOST_TEST(!dut);
  }

  {
    micro::StaticPtr<Simple, 64> dut(5, true, 1.0);
    BOOST_TEST(!!dut);
    BOOST_TEST(dut->a == 5);
    BOOST_TEST(dut->b == true);
    BOOST_TEST(dut->c == 1.0);

    [](const auto& foo) {
      // And it still works when accessed as const.
      BOOST_TEST(foo->a == 5);
      BOOST_TEST(foo->b == true);
      BOOST_TEST(foo->c == 1.0);
    }(dut);

    micro::StaticPtr<Simple, 64> empty;
    dut = std::move(empty);
    BOOST_TEST(!dut);
    BOOST_TEST(!empty);
  }

  {
    int destruct_count = 0;
    {
      micro::StaticPtr<NontrivialDestructor, 64> dut{&destruct_count};
      BOOST_TEST(destruct_count == 0);
    }
    BOOST_TEST(destruct_count == 1);
  }
}

BOOST_AUTO_TEST_CASE(MoveConstructFromEmptyStaticPtr) {
  // Move-construction from an empty StaticPtr used to call get() on
  // the empty source (returns nullptr) and then placement-new at the
  // empty destination's null get() — UB and a segfault on host.
  micro::StaticPtr<int, 16> empty;
  BOOST_TEST(!empty);

  micro::StaticPtr<int, 16> moved(std::move(empty));
  BOOST_TEST(!moved);
  BOOST_TEST(!empty);

  // std::swap on two empties uses the move constructor; it must not
  // crash either.
  micro::StaticPtr<int, 16> a;
  micro::StaticPtr<int, 16> b;
  std::swap(a, b);
  BOOST_TEST(!a);
  BOOST_TEST(!b);
}

BOOST_AUTO_TEST_CASE(SwapMember) {
  // The member swap() previously failed to compile when both sides
  // were populated (std::swap(*rhs, *this) mixed T& with StaticPtr&).
  // It is now exercised here for all four populated/empty combos.
  {
    micro::StaticPtr<int, 16> a(1);
    micro::StaticPtr<int, 16> b(2);
    a.swap(b);
    BOOST_TEST(*a == 2);
    BOOST_TEST(*b == 1);
  }
  {
    micro::StaticPtr<int, 16> a(1);
    micro::StaticPtr<int, 16> b;
    a.swap(b);
    BOOST_TEST(!a);
    BOOST_TEST(*b == 1);
  }
  {
    micro::StaticPtr<int, 16> a;
    micro::StaticPtr<int, 16> b(2);
    a.swap(b);
    BOOST_TEST(*a == 2);
    BOOST_TEST(!b);
  }
  {
    micro::StaticPtr<int, 16> a;
    micro::StaticPtr<int, 16> b;
    a.swap(b);
    BOOST_TEST(!a);
    BOOST_TEST(!b);
  }
}
