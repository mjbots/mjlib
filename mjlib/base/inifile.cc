// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/inifile.h"

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>

#include "mjlib/base/system_error.h"

namespace mjlib {
namespace base {

std::vector<std::string> ReadIniOptionStream(std::istream& istr) {
  std::vector<std::string> result;

  std::string context;
  auto add_context = [&](auto value) {
    if (context.empty()) { return value; }
    return context + "." + value;
  };

  int line_number = 0;

  while (istr) {
    line_number++;
    std::string line;
    std::getline(istr, line);

    // Strip off any trailing comments.
    const auto comment_char = line.find('#');
    if (comment_char != line.npos) {
      line = line.substr(0, comment_char);
    }

    // Trim leading or trailing whitespace.
    line = boost::trim_copy(line);

    if (line.empty()) { continue; }

    // If it starts with a brace, then we are entering a new section.
    if (line.front() == '[' && line.back() == ']') {
      context = line.substr(1, line.size() - 2);
      continue;
    }

    // Otherwise, it had better be an assignment.
    const auto equal_char = line.find('=');
    if (equal_char == line.npos) {
      throw system_error::einval(
          fmt::format("Error parsing, missing '=' on line {}", line_number));
    }

    const auto key = boost::trim_copy(line.substr(0, equal_char));
    const auto value = boost::trim_copy(line.substr(equal_char + 1));
    result.push_back(fmt::format("--{}", add_context(key)));
    result.push_back(value);
  }

  return result;
}

}
}
