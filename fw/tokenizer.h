// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "string_view.h"

/// A simple gsl::string_span based tokenizer.  It can split on
/// multiple delimiters, and reports multiple consecutive delimiters
/// as empty tokens.
class Tokenizer {
 public:
  Tokenizer(const string_view& source, const char* delimiters)
      : source_(source),
        delimiters_(delimiters),
        position_(source_.cbegin()) {}

  string_view next() {
    if (position_ == source_.end()) { return string_view(); }
    const auto start = position_;
    auto next = position_;
    bool found = false;
    for (; next != source_.end(); ++next) {
      if (std::strchr(delimiters_, *next) != nullptr) {
        position_ = next;
        ++position_;
        found = true;
        break;
      }
    }
    if (!found) { position_ = next; }
    return string_view(start, next);
  }

  gsl::cstring_span remaining() const {
    return string_view(position_, source_.end());
  }

 private:
  const string_view source_;
  const char* const delimiters_;
  string_view::const_iterator position_;
};
