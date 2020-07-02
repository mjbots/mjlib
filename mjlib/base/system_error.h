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

#pragma once

#include <string_view>

#include "mjlib/base/error_code.h"

namespace mjlib {
namespace base {

/// Similar to a boost::system::system_error, but works with
/// mjlib::base::error_code.
class system_error : public std::runtime_error {
 public:
  system_error(const error_code& ec)
      : std::runtime_error(""),
        ec_(ec) {}

  system_error(int val, const boost::system::error_category& category,
               const std::string& message = "")
      : system_error(error_code(val, category, message)) {}

  static system_error einval(const std::string& message) {
    return system_error(error_code::einval(message));
  }

  static system_error syserrno(const std::string& message) {
    return system_error(error_code::syserrno(message));
  }

  static void throw_if(bool value, std::string_view message = "") {
    if (value) {
      throw syserrno(std::string(message));
    }
  }

  virtual ~system_error() throw() {}

  const char* what() const throw() {
    if (message_.empty()) { message_ = ec_.message(); }
    return message_.c_str();
  }
  error_code& code() { return ec_; }
  const error_code& code() const { return ec_; }

 private:
  error_code ec_;
  mutable std::string message_;
};

}
}
