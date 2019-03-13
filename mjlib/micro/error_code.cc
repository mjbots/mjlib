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

#include "mjlib/micro/error_code.h"

namespace mjlib {
namespace micro {

error_condition error_category::default_error_condition(int code) const noexcept {
  return error_condition(code, *this);
}

bool error_category::equivalent(int code, const error_condition& other) const noexcept {
  return default_error_condition(code) == other;
}

bool error_category::equivalent(const error_code& code, int condition) const noexcept {
  return *this == code.category() && code.value() == condition;
}

bool error_category::operator==(const error_category& other) const noexcept {
  return this == &other;
}

bool error_category::operator!=(const error_category& other) const noexcept {
  return this != &other;
}

namespace {
class GenericCategory : public error_category {
 public:
  const char* name() const noexcept override { return "generic"; }
  std::string_view message(int condition) const override {
    if (condition == 0) {
      return "success";
    }
    return "unknown";
  }
};
}

const error_category& generic_category() noexcept {
  static GenericCategory result;
  return result;
}

}
}
