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

#include <iostream>
#include <string>

#include <boost/system/error_code.hpp>

namespace mjlib {
namespace base {

/// Similar to a boost::system::error_code, but provides a facility
/// for attaching additional context rather than just the error code.
class error_code {
 public:
  /// This constructor is purposefully non-explicit, so that we can
  /// transparently capture the std variety and later allow context
  /// to be added.
  error_code(const boost::system::error_code& ec,
             const std::string& message = "");

  error_code(int val, const boost::system::error_category& category,
             const std::string& message = "")
      : error_code(boost::system::error_code(val, category), message) {}

  template <typename ErrorCodeEnum>
  error_code(ErrorCodeEnum value, const std::string& message = "")
      : error_code(boost::system::error_code(value), message) {}

  error_code() {}

  static error_code einval(const std::string& message) {
    return error_code(boost::system::errc::invalid_argument,
                     boost::system::generic_category(),
                     message);
  }

  static error_code syserrno(const std::string& message) {
    return error_code(errno, boost::system::system_category(), message);
  }

  /// @return a string describing the message, along with all context
  /// which has been added.
  std::string message() const;

  /// @return the boost error_code associated with this error.
  const boost::system::error_code& boost_error_code() const { return ec_; }

  operator boost::system::error_code() const { return ec_; }
  explicit operator bool() const { return !!ec_; }

  bool operator==(const error_code& rhs) const {
    return ec_ == rhs.ec_;
  }

  bool operator!=(const error_code& rhs) const {
    return ec_ != rhs.ec_;
  }

  // Append context to this error.  These all boil down to adding
  // additional textual context.
  void AppendError(const error_code& ec) { Append(ec.message()); }
  void Append(const boost::system::error_code& ec) { Append(ec.message()); }
  void Append(const std::string& message) {
    if (!message_.empty()) { message_ += "\n"; }
    message_ += message;
  }

 private:
  boost::system::error_code ec_;
  std::string message_;
};

inline std::ostream& operator<<(std::ostream& ostr, const error_code& ec) {
  ostr << ec.message();
  return ostr;
}

}
}
