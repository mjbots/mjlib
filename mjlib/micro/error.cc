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

#include "mjlib/micro/error.h"

namespace mjlib {
namespace micro {

namespace {
struct MicroErrorCategory : error_category {
  const char* name() const noexcept override { return "mjlib.micro"; }
  std::string_view message(int condition) const override {
    switch (static_cast<errc>(condition)) {
      case errc::kDelimiterNotFound: return "delimiter not found";
    }
    return "unknown";
  }
};

const error_category& micro_error_category() {
  static MicroErrorCategory result;
  return result;
}
}

error_code make_error_code(errc err) {
  return error_code(static_cast<int>(err), micro_error_category());
}

}
}
