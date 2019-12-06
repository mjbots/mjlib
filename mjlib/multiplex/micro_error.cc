// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/micro_error.h"

namespace mjlib {
namespace multiplex {

namespace {
struct ErrorCategory : micro::error_category {
  const char* name() const noexcept override { return "mjlib.multiplex"; }
  std::string_view message(int condition) const override {
    switch (static_cast<errc>(condition)) {
      case errc::kPayloadTruncated: return "payload truncated";
    }
    return "unknown";
  }
};

const micro::error_category& get_error_category() {
  static ErrorCategory result;
  return result;
}
}

micro::error_code make_error_code(errc err) {
  return micro::error_code(static_cast<int>(err), get_error_category());
}

}
}
