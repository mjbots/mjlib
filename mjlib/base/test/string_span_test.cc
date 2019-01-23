// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/string_span.h"

#include <boost/test/auto_unit_test.hpp>

using mjlib::base::string_span;

BOOST_AUTO_TEST_CASE(BasicStringSpan) {
  {
    string_span empty;
    BOOST_TEST(empty.data() == nullptr);
    BOOST_TEST(empty.size() == 0);
    BOOST_TEST(empty.length() == 0);
    BOOST_TEST(empty.empty() == true);
  }

  {
    char data[] = "stuff";
    auto span = string_span::ensure_z(data);
    BOOST_TEST(span.size() == 5);
    BOOST_TEST(span.length() == 5);
    BOOST_TEST(span.empty() == false);
    BOOST_TEST(span[0] == 's');
    BOOST_TEST(span(0) == 's');

    int count = 0;
    for (char c: span) {
      BOOST_TEST(c != 0);
      count++;
    }
    BOOST_TEST(count == 5);

    span[1] = 'd';
    BOOST_TEST(data[1] == 'd');
  }
}
