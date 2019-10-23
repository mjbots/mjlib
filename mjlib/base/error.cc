// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/error.h"

namespace mjlib {
namespace base {

namespace {
class ErrorCategory : public boost::system::error_category {
 public:
  const char* name() const noexcept override {
    return "mjlib.base";
  }

  std::string message(int ev) const override {
    switch (static_cast<error>(ev)) {
      case error::kJsonParse: return "JSON parse error";
    }
    return "unknown";
  }
};

const ErrorCategory& make_error_category() {
  static ErrorCategory result;
  return result;
}
}  // namespace

boost::system::error_code make_error_code(error err) {
  return boost::system::error_code(static_cast<int>(err), make_error_category());
}

}
}
