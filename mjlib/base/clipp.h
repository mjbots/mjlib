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

/// @file
///
/// These are convenience functions for parsing command line arguments
/// with clipp.

#pragma once

#include <iostream>

#include <clipp/clipp.h>

#include "mjlib/base/inifile.h"

namespace mjlib {
namespace base {

inline clipp::doc_formatting UsageFormat() {
  return clipp::doc_formatting()
      .first_column(2)
      .doc_column(40);
}

template <typename Token>
void EmitUsage(std::ostream& ostr, const Token& token, clipp::doc_formatting format = UsageFormat()) {
  ostr << "Usage:\n"
       << clipp::documentation(
           token, format, clipp::param_filter{}.has_doc(clipp::tri::either))
       << "\n";
}

template <typename Token>
void ClippParse(int argc, char** argv, const Token& token) {
  clipp::group group;
  group = (
      clipp::option("-h", "--help").call([&]() {
          EmitUsage(std::cout, group);
          std::exit(0);
        }),
      clipp::with_prefixes_short_long("-", "--", token)
  );
  if (!clipp::parse(argc, argv, group)) {
    std::cerr << "Invalid options, try --help\n";
    std::exit(1);
  }
}

template <typename Token>
void ClippParseIni(std::istream& istr, const Token& token) {
  std::vector<std::string> tokenize = ReadIniOptionStream(istr);
  clipp::parse(tokenize.begin(), tokenize.end(),
               clipp::with_prefixes_short_long("-", "--", token));
}

}
}
