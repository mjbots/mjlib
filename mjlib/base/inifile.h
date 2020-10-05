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

/// @file
///
/// These are convenience functions for parsing command line arguments
/// with clipp.

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace mjlib {
namespace base {

/// Read a .ini file assuming it describes hierarchical command line
/// options.  Return the result as a series of option tokens.
std::vector<std::string> ReadIniOptionStream(std::istream&);

}
}
