// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <fmt/format.h>

#include "mjlib/base/bytes.h"
#include "mjlib/base/error.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/system_error.h"
#include "mjlib/base/visit_archive.h"

namespace mjlib {
namespace base {

class Json5ReadArchive : public VisitArchive<Json5ReadArchive> {
 public:
  struct Options {
    /// Allow "null" as a respelling of NaN for numbers.
    bool permissive_nan = false;

    Options() {}

    Options& set_permissive_nan(bool value) {
      permissive_nan = value;
      return *this;
    }
  };

  Json5ReadArchive(std::istream& istr,
                   const Options& options = Options())
      : istr_(istr), options_(options) {}

  template <typename Serializable>
  Json5ReadArchive& Accept(Serializable* serializable) {
    // The first token that must be present is the open brace.
    ReadLiteral("{");

    Prepare_JSON5Member();
    while (!done_) {
      any_found_ = false;
      VisitArchive<Json5ReadArchive>::Accept(serializable);
      if (!any_found_) {
        Ignore_JSON5Value();
        IgnoreWhitespace();

        if (Peek() != ',') {
          ReadLiteral("}");
          done_ = true;
        } else {
          Get();
          Prepare_JSON5Member();
        }
      }
    }

    return *this;
  }

  template <typename ValueType>
  static ValueType Read(std::istream& istr,
                        const Options& options = Options()) {
    ValueType result;
    Json5ReadArchive(istr, options).Value(&result);
    return result;
  }

  template <typename ValueType>
  static ValueType Read(const std::string_view& str,
                        const Options& options = Options()) {
    std::istringstream istr{std::string(str)};
    return Read<ValueType>(istr, options);
  }

  template <typename ValueType>
  Json5ReadArchive& Value(ValueType* value) {
    ReferenceNameValuePair nvp(value, "");
    VisitArchive<Json5ReadArchive>::Visit(nvp);
    return *this;
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& nvp) {
    if (done_) { return; }
    if (nvp.name() != current_field_name_) { return; }

    any_found_ = true;

    VisitArchive<Json5ReadArchive>::Visit(nvp);

    IgnoreWhitespace();
    if (Peek() != ',') {
      ReadLiteral("}");
      done_ = true;
    } else {
      Get();  // Eat the comma.
      Prepare_JSON5Member();
    }
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    Json5ReadArchive sub_archive(istr_, options_);
    sub_archive.Accept(nvp.value());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp,
                        NameMapGetter enumeration_mapper) {
    const auto text = Read_JSON5String();
    *nvp.value() = [&]() {
      for (const auto& pair : enumeration_mapper()) {
        if (pair.second == text) {
          return pair.first;
        }
      }
      Error(fmt::format("Invalid enumeration value '{}'", text));
    }();
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    VisitHelper(nvp, nvp.value(), base::PriorityTag<2>());
  }

 private:
  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair&,
                   std::map<std::string, T>* value,
                   base::PriorityTag<2>) {
    ReadLiteral("{");

    value->clear();

    while (true) {
      IgnoreWhitespace();

      const auto next = Peek();
      if (next == '}') {
        ReadLiteral("}");
        return;
      }

      const std::string name = Read_JSON5MemberName();
      ReadLiteral(":");
      IgnoreWhitespace();

      T item{};
      Value(&item);

      value->insert(std::make_pair(name, std::move(item)));
      IgnoreWhitespace();

      if (Peek() != ',') {
        ReadLiteral("}");
        return;
      } else {
        ReadLiteral(",");
      }
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   Bytes* value,
                   base::PriorityTag<2> tag) {
    VisitHelper(nvp, static_cast<std::vector<uint8_t>*>(value), tag);
  }

  template <typename NameValuePair, typename T, size_t N>
  void VisitHelper(const NameValuePair&,
                   std::array<T, N>* value,
                   base::PriorityTag<1>) {
    size_t pos = 0;
    ReadArray<T>([&]() -> T& {
        auto& result = (*value)[pos];
        pos++;
        if (pos > N) {
          Error("Too many elements in array");
        }
        return result;
      });
    if (pos < N) {
      Error("Too few elements in array");
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp,
                   std::vector<T>*,
                   base::PriorityTag<1>) {
    auto* result = nvp.value();
    size_t pos = 0;
    ReadArray<T>([&]() -> T& {
        const auto index = pos;
        if (index >= result->size()) { result->push_back(T()); }
        pos++;
        return (*result)[index];
      });
  }

  template <typename T, typename Callback>
  void ReadArray(Callback callback) {
    ReadLiteral("[");

    while (true) {
      IgnoreWhitespace();
      {
        const auto maybe_end = Peek();
        if (maybe_end == ']') {
          Get();
          break;
        }
      }

      T& value = callback();
      Value(&value);

      IgnoreWhitespace();
      const auto maybe_comma = Peek();
      if (maybe_comma == ',') { Get(); }

      IgnoreWhitespace();
      {
        const auto maybe_end = Peek();
        if (maybe_end == ']') {
          Get();
          break;
        }
      }
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp,
                   std::optional<T>*,
                   base::PriorityTag<1>) {
    const auto next = Peek();
    if (next == 'n') {
      ReadLiteral("null");
      nvp.set_value(std::optional<T>{});
    } else {
      if (nvp.value()->has_value()) {
        Value(&nvp.value()->value());
      } else {
        T value = {};
        Value(&value);
        nvp.set_value(value);
      }
    }
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::ptime*,
                   base::PriorityTag<1>) {
    nvp.set_value(boost::posix_time::time_from_string(
                      Read_JSON5String()));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   boost::posix_time::time_duration*,
                   base::PriorityTag<1>) {
    nvp.set_value(boost::posix_time::duration_from_string(
                      Read_JSON5String()));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   std::string*,
                   base::PriorityTag<1>) {
    nvp.set_value(Read_JSON5String());
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   float*,
                   base::PriorityTag<1>) {
    nvp.set_value(ToFloat<float>(Read_JSON5Number().text));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   double*,
                   base::PriorityTag<1>) {
    nvp.set_value(ToFloat<double>(Read_JSON5Number().text));
  }

  template <typename NameValuePair>
  void VisitHelper(const NameValuePair& nvp,
                   bool*,
                   base::PriorityTag<1>) {
    const auto next = Peek();
    if (next == 't') {
      ReadLiteral("true");
      nvp.set_value(true);
    } else if (next == 'f') {
      ReadLiteral("false");
      nvp.set_value(false);
    } else {
      Error("Unknown boolean value");
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& nvp,
                   T*,
                   base::PriorityTag<0>) {
    if constexpr (std::is_signed<T>::value) {
      nvp.set_value(CheckLimit<T>(Read_JSON5SignedInteger()));
    } else {
      nvp.set_value(CheckLimit<T>(Read_JSON5UnsignedInteger()));
    }
  }

  template <typename T, typename InType>
  T CheckLimit(InType in) {
    if (in < std::numeric_limits<T>::min() ||
        in > std::numeric_limits<T>::max()) {
      Error("Value out of bounds");
    }
    return in;
  }

  void Prepare_JSON5Member() {
    IgnoreWhitespace();
    auto next = Peek();
    if (next == '}') {
      ReadLiteral("}");
      done_ = true;
      return;
    }

    current_field_name_ = Read_JSON5MemberName();
    ReadLiteral(":");
    IgnoreWhitespace();
  }

  std::string Read_JSON5MemberName() {
    IgnoreWhitespace();
    const auto c = Peek();
    if (c == '"') {
      return Read_JSON5String();
    } else {
      return Read_JSON5Identifier();
    }
  }

  std::string Read_JSON5Identifier() {
    // For now, we're going to ignore the possibility of unicode
    // characters in identifiers.
    std::ostringstream ostr;
    auto first = Get();
    if (!IsIdentifierFirstCharacter(first)) {
      Error(fmt::format("Incorrect first character of identifer: '{}'",
                        static_cast<char>(first)));
    }
    ostr.put(first);

    while (IsIdentifierCharacter(Peek())) {
      ostr.put(Get());
    }
    return ostr.str();
  }

  std::string Read_JSON5String() {
    const auto first = Get();
    if (first == '"') {
      return Read_JSON5DoubleString();
    } else if (first == '\'') {
      return Read_JSON5SingleString();
    }

    Error(fmt::format("Unexpected start of string '{}'",
                      static_cast<char>(first)));
  }

  std::string Read_JSON5DoubleString() {
    return Read_JSON5TerminatedString('"');
  }

  std::string Read_JSON5SingleString() {
    return Read_JSON5TerminatedString('\'');
  }

  std::string Read_JSON5TerminatedString(char terminator) {
    std::ostringstream ostr;
    while (true) {
      const auto c = Get();
      if (c == terminator) {
        return ostr.str();
      } else if (c == '\\') {
        // Some type of escape sequence.
        ostr << Read_EscapeSequence();
      } else if (c >= 0x80) {
        // Some UTF8 code point.
        AssertNotReached();
      } else {
        ostr.put(c);
      }
    }
  }

  std::string Read_EscapeSequence() {
    const auto first = Get();
    if (first == '\\') { return "\\"; }
    else if (first == '\'') { return "'"; }
    else if (first == '\"') { return "\""; }
    else if (first == 'b') { return "\b"; }
    else if (first == 'f') { return "\f"; }
    else if (first == 'n') { return "\n"; }
    else if (first == 'r') { return "\r"; }
    else if (first == 't') { return "\t"; }
    else if (first == 'v') { return "\v"; }
    else if (first == 'x') {
      const auto char1 = Get();
      const auto char2 = Get();
      return std::string(
          1, static_cast<char>(
              std::stol(
                  (std::string(1, static_cast<char>(char1)) +
                   static_cast<char>(char2)),
                  0, 16)));
    }
    Error(fmt::format("Escape not handled '{}'", static_cast<char>(first)));
  }

  int64_t Read_JSON5SignedInteger() {
    const auto number = Read_JSON5Number();
    std::size_t pos = 0;
    try {
      const auto result = std::stoll(number.text, &pos, number.base);

      if (pos != number.text.size()) {
        Error(fmt::format("Could not interpret '{}' as an integer",
                        number.text));
      }
      return result;
    } catch (std::invalid_argument& e) {
      Error(fmt::format("Error parsing integer: {}", e.what()));
    } catch (std::out_of_range& e) {
      Error(fmt::format("Integer out of range: {}", e.what()));
    }
  }

  uint64_t Read_JSON5UnsignedInteger() {
    const auto number = Read_JSON5Number();
    std::size_t pos = 0;
    const auto result = std::stoull(number.text, &pos, number.base);

    if (pos != number.text.size()) {
      Error(fmt::format("Could not interpret '{}' as an integer",
                        number.text));
    }
    return result;
  }

  struct Number {
    std::string text;
    int base = 0;
  };

  Number Read_JSON5Number() {
    const auto first = Peek();
    if (first == '+' || first == '-') {
      const auto prefix = static_cast<char>(Get());
      auto result = Read_NumericLiteral();
      result.text = prefix + result.text;
      return result;
    } else {
      return Read_NumericLiteral();
    }
  }

  Number Read_NumericLiteral() {
    const auto first = Peek();
    if (first == 'I') {
      ReadLiteral("Infinity");
      return {"Infinity"};
    } else if (first == 'N') {
      ReadLiteral("NaN");
      return {"NaN"};
    } else if (options_.permissive_nan && first == 'n') {
      ReadLiteral("null");
      return {"NaN"};
    }

    if (first != '0') {
      // We are definitely dealing with a DecimalLiteral.
      return {Read_DecimalLiteral(false)};
    }
    Get();

    // first == '0' here
    const auto next = Peek();
    if (next == 'b' || next == 'B') {
      Get();
      return {Read_BinaryIntegerLiteral(), 2};
    } else if (next == 'o' || next == 'O') {
      Get();
      return {Read_OctalIntegerLiteral(), 8};
    } else if (next == 'x' || next == 'X') {
      Get();
      return {Read_HexIntegerLiteral(), 16};
    }

    // Guess we were a DecimalLiteral after all that happened to start
    // with 0.
    return {Read_DecimalLiteral(true)};
  }

  std::string Read_DecimalLiteral(bool initial_zero) {
    std::ostringstream ostr;

    const auto first = initial_zero ? '0' : Peek();
    if (first != '.') {
      ostr << Read_DecimalIntegerLiteral(initial_zero);
    }

    const auto next = Peek();
    if (next == '.') {
      ostr.put(Get());
      ostr << Read_DecimalDigits();
      ostr << Read_ExponentPart();
    } else if (next == 'e' || next == 'E') {
      ostr << Read_ExponentPart();
    }

    return ostr.str();
  }

  std::string Read_DecimalIntegerLiteral(bool initial_zero) {
    if (initial_zero) {
      // Our parent already read this.
      return "0";
    }

    return Read_DecimalDigits();
  }

  std::string Read_DecimalDigits() {
    std::ostringstream ostr;
    while (true) {
      const auto c = Peek();
      if (c < '0' || c > '9') { return ostr.str(); }
      ostr.put(Get());
    }
    return ostr.str();
  }

  std::string Read_ExponentPart() {
    const auto peek = Peek();
    if (peek != 'e' && peek != 'E') { return ""; }
    const auto first = Get();
    const auto digits = Read_SignedInteger();
    return static_cast<char>(first) + digits;
  }

  std::string Read_SignedInteger() {
    const auto peek = Peek();
    if (peek == '+' || peek == '-') {
      const auto first = Get();
      const auto digits = Read_DecimalDigits();
      return static_cast<char>(first) + digits;
    }
    return Read_DecimalDigits();
  }

  std::string Read_BinaryIntegerLiteral() {
    std::ostringstream ostr("0b");
    while (true) {
      const auto c = Peek();
      if (c != '0' && c != '1') { return ostr.str(); }
      ostr.put(Get());
    }
  }

  std::string Read_OctalIntegerLiteral() {
    std::ostringstream ostr("0o");
    while (true) {
      const auto c = Peek();
      if (c < '0' || c > '7') { return ostr.str(); }
      ostr.put(Get());
    }
  }

  std::string Read_HexIntegerLiteral() {
    std::ostringstream ostr("0x");
    while (true) {
      const auto c = Peek();
      if (!((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) {
        return ostr.str();
      }
      ostr.put(Get());
    }
  }

  void ReadLiteral(const char* literal) {
    const char* original = literal;
    IgnoreWhitespace();
    while (*literal != 0) {
      const auto c = Get();
      if (c != *literal) {
        Error(fmt::format("Didn't find expected '{}'", original));
      }
      literal++;
    }
  }

  void Ignore_JSON5Value() {
    const auto next = Peek();
    // Let's see what we need to ignore.
    if (next == '"') {
      Read_JSON5String();
    } else if (next == '[') {
      Ignore_JSON5Array();
    } else if (next == '{') {
      Ignore_JSON5Object();
    } else if (next == 't') {
      ReadLiteral("true");
    } else if (next == 'f') {
      ReadLiteral("false");
    } else if (next == 'n') {
      ReadLiteral("null");
    } else {
      // Guess it must be a number.
      Read_JSON5Number();
    }
  }

  void Ignore_JSON5Array() {
    ReadLiteral("[");
    while (true) {
      IgnoreWhitespace();

      {
        const auto next = Peek();
        if (next == ']') {
          Get();
          return;
        }
      }

      Ignore_JSON5Value();

      IgnoreWhitespace();

      {
        const auto next = Peek();

        if (next == ',') {
          Get();
          continue;
        } else if (next == ']') {
          Get();
          return;
        } else {
          Error("Array syntax error");
        }
      }
    }
  }

  void Ignore_JSON5Object() {
    ReadLiteral("{");
    while (true) {
      IgnoreWhitespace();

      {
        const auto next = Peek();
        if (next == '}') {
          Get();
          return;
        }
      }

      Read_JSON5MemberName();
      IgnoreWhitespace();
      ReadLiteral(":");

      IgnoreWhitespace();
      Ignore_JSON5Value();
      IgnoreWhitespace();

      {
        const auto next = Peek();
        if (next == ',') {
          Get();
          continue;
        } else if (next == '}') {
          Get();
          return;
        } else {
          Error("Object syntax error");
        }
      }
    }
  }

  static bool IsIdentifierFirstCharacter(int c) {
    return std::isalpha(c) ||
        (c == '$' ||
         c == '_');
  }

  static bool IsIdentifierCharacter(int c) {
    return IsIdentifierFirstCharacter(c) ||
        std::isdigit(c);
  }

  static bool IsWhitespace(int c) {
    return (c == 0x09 ||
            c == 0x0a ||
            c == 0x0b ||
            c == 0x0c ||
            c == 0x0d ||
            c == 0x20 ||
            c == 0xa0);
  }

  void IgnoreWhitespace() {
    while (true) {
      const auto c = Peek();
      if (IsWhitespace(c)) {
        Get();
      } else if (c == '/') {
        // In a region where whitespace is allowed, this must either
        // be an error, or the start of some kind of comment.
        Get();
        auto next = Get();
        if (next == '/') {
          // A single line comment.
          ReadUntilOneOf("\x0a\x0d");
        } else if (next == '*') {
          // A multi-line comment.
          ReadUntil("*/");
        } else {
          Error(fmt::format("Unexpected '/' character followed by '{}'",
                            static_cast<char>(next)));
        }
      } else {
        return;
      }
    }
  }

  void ReadUntilOneOf(const char* possible) {
    while (true) {
      const auto c = Get();
      if (std::strchr(possible, c) != nullptr) { return; }
    }
  }

  void ReadUntil(const char* terminus) {
    const char* current = terminus;
    while (true) {
      const auto c = Get();
      if (c == *current) {
        current++;
        if (*current == 0) { return; }
      } else {
        current = terminus;
      }
    }
  }

  int Get() {
    const auto result = istr_.get();
    if (istr_.eof()) { Error("EOF"); }

    if (result == '\n') {
      line_++;
      column_ = 0;
    } else {
      column_++;
    }

    return result;
  }

  int Peek() {
    return istr_.peek();
  }

  void Error(const std::string& error) const __attribute__ ((noreturn)) {
    throw system_error(
        error_code(
            error::kJsonParse,
            fmt::format("{}:{} {}", line_, column_, error)));
  }

  template <typename T>
  T ToFloat(const std::string& str) const {
    if (str == "Infinity" || str == "+Infinity") {
      return std::numeric_limits<T>::infinity();
    }
    if (str == "-Infinity") {
      return -std::numeric_limits<T>::infinity();
    }
    if (str == "NaN") {
      return std::numeric_limits<T>::quiet_NaN();
    }
    try {
      return std::stod(str);
    } catch (std::invalid_argument& e) {
      Error(fmt::format("Error parsing number: {}", e.what()));
    } catch (std::out_of_range& e) {
      Error(fmt::format("Number out of range: {}", e.what()));
    }
  }

  std::istream& istr_;
  const Options options_;
  bool done_ = false;
  std::string current_field_name_;
  bool any_found_ = false;

  int line_ = 1;
  int column_ = 0;
};


}
}
