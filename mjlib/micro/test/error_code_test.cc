// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/error_code.h"

#include <boost/test/auto_unit_test.hpp>

namespace {
namespace my_error {

enum class Errc {
  kSuccess = 0,
  kError1,
  kError2,
};

class MyCategory : public mjlib::micro::error_category {
 public:
  const char* name() const noexcept override { return "my_category"; }
  std::string_view message(int condition) const override {
    switch (static_cast<Errc>(condition)) {
      case Errc::kSuccess: { return "success"; }
      case Errc::kError1: { return "error1"; }
      case Errc::kError2: { return "error2"; }
    }
  }
};

const mjlib::micro::error_category& my_category() noexcept {
  static MyCategory result;
  return result;
}

mjlib::micro::error_code make_error_code(Errc errc) {
  return {static_cast<int>(errc), my_category()};
}

}
}

namespace mjlib {
namespace micro {
template <>
struct is_error_code_enum<my_error::Errc> : std::true_type {};
}
}

BOOST_AUTO_TEST_CASE(BasicErrorCode) {
  using mjlib::micro::error_code;

  // Default constructor.
  {
    error_code ec;
    BOOST_TEST(ec.value() == 0);
    BOOST_TEST((ec.category() == mjlib::micro::generic_category()));

    BOOST_TEST(ec == ec);
    error_code other;
    BOOST_TEST(ec == other);
    BOOST_TEST(!(ec != other));

    BOOST_TEST(!ec);
  }

  // Constructed from our enum.
  {
    error_code ec = my_error::Errc::kError1;
    BOOST_TEST(ec.value() == static_cast<int>(my_error::Errc::kError1));
    BOOST_TEST((ec.category() == my_error::my_category()));
    BOOST_TEST(!!ec);

    error_code success;
    BOOST_TEST(ec != success);
    BOOST_TEST(ec == ec);

    error_code ec2 = my_error::Errc::kError2;
    BOOST_TEST(ec != ec2);

    error_code ec1 = my_error::Errc::kError1;
    BOOST_TEST(ec == ec1);
    BOOST_TEST(ec1 != ec2);
  }
}
