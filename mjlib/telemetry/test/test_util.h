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

#include <fmt/format.h>

namespace mjlib {
namespace telemetry {
namespace test {

template <typename T>
inline std::string Hexify(const T& value) {
  std::string result;
  size_t count = 0;
  for (const auto& item : value) {
    result += fmt::format("{:02x}", static_cast<uint8_t>(item));
    count++;
    if ((count % 4) == 0) { result += " "; }
  }
  return result;
}

template <typename T1, typename T2>
void Compare(const T1& lhs, const T2& rhs) {
  BOOST_TEST(Hexify(lhs) == Hexify(rhs));
}

}
}
}
