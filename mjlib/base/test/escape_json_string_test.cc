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

#include "mjlib/base/escape_json_string.h"

#include <string>

#include <boost/test/auto_unit_test.hpp>

namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(EscapeJsonStringNamedEscapes) {
  BOOST_TEST(base::EscapeJsonString("\"") == "\\\"");
  BOOST_TEST(base::EscapeJsonString("\\") == "\\\\");
  BOOST_TEST(base::EscapeJsonString("\b") == "\\b");
  BOOST_TEST(base::EscapeJsonString("\f") == "\\f");
  BOOST_TEST(base::EscapeJsonString("\n") == "\\n");
  BOOST_TEST(base::EscapeJsonString("\r") == "\\r");
  BOOST_TEST(base::EscapeJsonString("\t") == "\\t");
  BOOST_TEST(base::EscapeJsonString(std::string(1, '\0')) == "\\u0000");
}

BOOST_AUTO_TEST_CASE(EscapeJsonStringPrintableAsciiPasses) {
  BOOST_TEST(base::EscapeJsonString("hello world") == "hello world");
  BOOST_TEST(base::EscapeJsonString(" ") == " ");
  BOOST_TEST(base::EscapeJsonString("~") == "~");
}

BOOST_AUTO_TEST_CASE(EscapeJsonStringEscapesAllControlChars) {
  // RFC 8259 sec 7 requires every byte in U+0000..U+001F to be escaped.
  // Previously only \b \t \n \f \r and NUL were handled; the remaining
  // 26 control bytes fell through to a verbatim copy.
  for (int c = 0x00; c <= 0x1F; ++c) {
    const std::string in(1, static_cast<char>(c));
    const std::string out = base::EscapeJsonString(in);
    BOOST_TEST(out.size() >= 2u,
               "control char 0x" << std::hex << c
               << " produced output of size " << out.size());
    BOOST_TEST(out[0] == '\\',
               "control char 0x" << std::hex << c
               << " did not produce a backslash escape");
  }
}

BOOST_AUTO_TEST_CASE(EscapeJsonStringEscEscaping) {
  // 0x1B (ESC) is the most common "in the wild" trigger via ANSI
  // escape sequences embedded in log strings.
  BOOST_TEST(base::EscapeJsonString("\x1b") == "\\u001b");
  BOOST_TEST(base::EscapeJsonString("\x01") == "\\u0001");
  BOOST_TEST(base::EscapeJsonString("\x1f") == "\\u001f");
}
