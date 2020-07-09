// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/escape_json_string.h"

#include <sstream>

namespace mjlib {
namespace base {

std::string EscapeJsonString(const std::string& in) {
  std::ostringstream out;
  for (char c : in) {
    switch (c) {
      case '"': {
        out << "\\\"";
        break;
      }
      case '\\': {
        out << "\\\\";
        break;
      }
      case '\b': {
        out << "\\b";
        break;
      }
      case '\f': {
        out << "\\f";
        break;
      }
      case '\n': {
        out << "\\n";
        break;
      }
      case '\r': {
        out << "\\r";
        break;
      }
      case '\t': {
        out << "\\t";
        break;
      }
      case 0: {
        out << "\\u0000";
        break;
      }
      default: {
        out << c;
        break;
      }
    }
  }

  return out.str();
}

}
}
