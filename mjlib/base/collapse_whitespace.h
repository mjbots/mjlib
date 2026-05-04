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

#pragma once

#include <cctype>

#include <sstream>
#include <string>

namespace mjlib {
namespace base {

inline std::string CollapseWhitespace(const std::string& str) {
  std::ostringstream ostr;
  bool was_whitespace = true;
  for (char c : str) {
    // std::isspace is UB for char arguments outside 0..UCHAR_MAX (i.e.
    // negative values when char is signed) -- the C library expects
    // either an unsigned char value or EOF. Cast first.
    const bool is_ws = std::isspace(static_cast<unsigned char>(c));
    if (!was_whitespace || !is_ws) {
      ostr.put(c);
    }
    was_whitespace = is_ws;
  }
  return ostr.str();
}

}
}
