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

// This include is intentionally first and alone, as the regression
// check for "header is not self-contained": the header must compile
// without relying on other test-driver headers having pulled <sstream>
// in transitively.
#include "mjlib/base/collapse_whitespace.h"

#include <boost/test/auto_unit_test.hpp>

// `clipp_archive_test.cc` (already in this binary) also includes
// collapse_whitespace.h. By having a second TU here that does the
// same, we force the linker to deduplicate definitions across TUs --
// pre-fix the function had external linkage and a multiple-definition
// error would surface here.

namespace base = mjlib::base;

BOOST_AUTO_TEST_CASE(CollapseWhitespaceBasic) {
  BOOST_TEST(base::CollapseWhitespace("") == "");
  BOOST_TEST(base::CollapseWhitespace("foo") == "foo");
  BOOST_TEST(base::CollapseWhitespace("foo bar") == "foo bar");
  // Internal runs collapse to a single whitespace character (the
  // first of the run).
  BOOST_TEST(base::CollapseWhitespace("foo  bar") == "foo bar");
  BOOST_TEST(base::CollapseWhitespace("foo\t\tbar") == "foo\tbar");
  // Leading whitespace is dropped; the first whitespace character of
  // a trailing run is preserved.
  BOOST_TEST(base::CollapseWhitespace("  foo bar") == "foo bar");
  BOOST_TEST(base::CollapseWhitespace("foo bar  ") == "foo bar ");

  // Non-ASCII (high-bit) bytes must pass through unchanged. char is
  // signed on most platforms, so calling std::isspace with these
  // bytes without first casting to unsigned char is undefined.
  BOOST_TEST(base::CollapseWhitespace("\xc3\xa9") == "\xc3\xa9");      // "é"
  BOOST_TEST(base::CollapseWhitespace("a\xc3\xa9 b") == "a\xc3\xa9 b");
}
