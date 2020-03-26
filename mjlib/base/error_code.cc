// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/error_code.h"

#include <sstream>

#include "mjlib/base/stringify.h"

namespace mjlib {
namespace base {

error_code::error_code(const boost::system::error_code& ec,
                       const std::string& message)
    : ec_(ec), message_(message) {}

std::string error_code::message() const {
  const auto result =
      (ec_ ? (Stringify(ec_) + " " + ec_.message()) +
       (message_.empty() ? "" : "\n"): "") + message_;
  return result;
}

}
}
