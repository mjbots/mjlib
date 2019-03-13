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

/// @file
///
/// This is a minimal re-implementation of std::error_code that does
/// not have a dependency upon std::string (and thus requires no
/// dynamic allocation).

#pragma once

#include <ostream>
#include <string_view>
#include <type_traits>

namespace mjlib {
namespace micro {
class error_condition;
class error_code;

class error_category {
 public:
  virtual ~error_category() {}
  error_category& operator=(const error_category&) = delete;

  virtual const char* name() const noexcept = 0;
  virtual error_condition default_error_condition(int code) const noexcept;
  virtual bool equivalent(int code, const error_condition&) const noexcept;
  virtual bool equivalent(const error_code&, int condition) const noexcept;

  virtual std::string_view message(int condition) const = 0;

  bool operator==(const error_category&) const noexcept;
  bool operator!=(const error_category&) const noexcept;
};

// Obtain a reference to the generic category.
const error_category& generic_category() noexcept;

template <typename T>
struct is_error_condition_enum : std::false_type {};

class error_condition {
 public:
  error_condition() noexcept {}
  error_condition(const error_condition&) noexcept = default;
  error_condition(int value, const error_category& category) noexcept
      : value_(value), category_(&category) {}

  template <class ErrorConditionEnum,
            std::enable_if_t<
              mjlib::micro::is_error_condition_enum<ErrorConditionEnum>::value,
              int*> = nullptr>
  error_condition(ErrorConditionEnum e) noexcept
      : error_condition(make_error_condition(e)) {}

  int value() const noexcept { return value_; }
  const error_category& category() const noexcept { return *category_; }

  bool operator==(const error_condition& rhs) const noexcept {
    return value_ == rhs.value_ && category_ == rhs.category_;
  }

  bool operator!=(const error_condition& rhs) const noexcept {
    return !(*this == rhs);
  }

 private:
  int value_{};
  const error_category* category_ = &generic_category();
};

template <typename T>
struct is_error_code_enum : std::false_type {};

class error_code {
 public:
  error_code() noexcept {}
  error_code(int ec, const error_category& category) noexcept
      : ec_(ec), category_(&category) {}

  template <class ErrorCodeEnum,
            std::enable_if_t<
              mjlib::micro::is_error_code_enum<ErrorCodeEnum>::value,
              int*> = nullptr>
  error_code(ErrorCodeEnum e) noexcept
      : error_code(make_error_code(e)) {}

  template <class ErrorCodeEnum,
            std::enable_if_t<
              mjlib::micro::is_error_code_enum<ErrorCodeEnum>::value,
              int*> = nullptr>
  error_code& operator=(ErrorCodeEnum e) {
    *this = make_error_code(e);
    return *this;
  }

  void assign(int ec, const error_category& category) noexcept {
    ec_ = ec;
    category_ = &category;
  }

  void clear() {
    ec_ = 0;
    category_ = &generic_category();
  }

  int value() const noexcept { return ec_; }
  const error_category& category() const noexcept { return *category_; }

  error_condition default_error_condition() const noexcept {
    return category_->default_error_condition(ec_);
  }

  std::string_view message() const {
    return category_->message(ec_);
  }

  explicit operator bool() const noexcept {
    return ec_ != 0;
  }

  bool operator==(const error_code& rhs) const noexcept {
    return ec_ == rhs.ec_ && category_ == rhs.category_;
  }

  bool operator!=(const error_code& rhs) const noexcept {
    return !(*this == rhs);
  }

 private:
  int ec_{};
  const error_category* category_ = &generic_category();
};

inline std::ostream& operator<<(std::ostream& ostr, const error_code& ec) {
  ostr << ec.category().name() << ":" << ec.value();
  return ostr;
}

}
}
