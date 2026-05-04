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

#include "mjlib/base/escape_json_string.h"

#include <cstdio>
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
      default: {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20) {
          // RFC 8259 sec 7 requires every control character in
          // U+0000..U+001F to be escaped. Anything not covered above
          // (NUL, VT, ESC, DEL-region controls, etc.) gets the
          // generic \u00xx form.
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", u);
          out << buf;
        } else {
          out << c;
        }
        break;
      }
    }
  }

  return out.str();
}

}
}
