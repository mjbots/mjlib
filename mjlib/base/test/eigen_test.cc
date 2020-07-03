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

#include "mjlib/base/eigen.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/json5_write_archive.h"

using namespace mjlib;

BOOST_AUTO_TEST_CASE(BasicEigenVisit) {
  Eigen::Matrix3d matrix;
  matrix(0, 0) = 1;
  matrix(0, 1) = 2;
  matrix(0, 2) = 3;
  matrix(2, 2) = 9;
  const auto actual = base::Json5WriteArchive::Write(matrix);
  const std::string expected = R"XXX([
    [
    1,
    0,
    0
  ],
    [
    2,
    0,
    0
  ],
    [
    3,
    0,
    9
  ]
  ])XXX";
  BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_CASE(EigenVectorVisit) {
  Eigen::Vector3d vector;
  vector(0) = 2;
  vector(1) = 5;
  vector(2) = 6;
  const auto actual = base::Json5WriteArchive::Write(vector);
  const std::string expected = R"XXX([
    2,
    5,
    6
  ])XXX";
  BOOST_TEST(actual == expected);
}

namespace {
struct StructWithEigen {
  Eigen::Vector3d vector;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(vector));
  }
};
}

BOOST_AUTO_TEST_CASE(EigenStruct) {
  StructWithEigen test;
  const auto actual = base::Json5WriteArchive::Write(test);
  const std::string expected = R"XXX({
    "vector" : [
      0,
      0,
      0
    ]
  })XXX";
  BOOST_TEST(actual == expected);
}
