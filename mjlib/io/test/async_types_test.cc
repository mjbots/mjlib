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

#include "mjlib/io/async_types.h"

#include <boost/test/auto_unit_test.hpp>

namespace {
class MoveOnly {
 public:
  MoveOnly() {}

  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  MoveOnly(MoveOnly&&) {}
  MoveOnly& operator=(MoveOnly&&) { return *this; }
};
}

BOOST_AUTO_TEST_CASE(AsyncTypesTest) {
  // We should be able to construct a WriteHandler from a move-only
  // type.
  MoveOnly move_only;
  mjlib::io::WriteHandler dut{[arg = std::move(move_only)](auto, auto) {}};
}
