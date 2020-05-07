// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include <string>
#include <vector>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/stream.h"
#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// Given a binary schema as defined in README.md, provide C++
/// mechanisms for reading the contained fields and extracting data
/// from data records formatted with this schema dynamically at
/// runtime.
class BinarySchemaParser {
 private:
  class Impl;
 public:
  BinarySchemaParser(std::string_view schema, std::string_view record_name = "");
  ~BinarySchemaParser();

  struct Element;

  struct Field {
    std::string name;
    std::vector<std::string> aliases;
    const Element* element = nullptr;

    std::string default_value;
  };

  struct Element {
    Format::Type type;

    /// The parent of this element, if one exists.
    const Element* parent = nullptr;

    /////////
    /// Available for all types.

    std::string name;

    /// If known, the fixed offset within a data record of this
    /// element.
    int64_t maybe_fixed_offset = -1;

    /////////
    /// Available for some types
    std::vector<std::string> aliases;

    /// for kFixedInt and kFixedUInt
    int int_size = -1;

    /// For kArray, kMap, kUnion, kEnum
    std::vector<const Element*> children;

    /// For kEnum
    std::map<uint64_t, std::string> enum_items;

    /// For kObject
    std::vector<Field> fields;
  };

  const Element* root() const;

  /// Given a particular element, and a read stream pointing to the
  /// beginning of a data record, skip to the beginning of that
  /// element.
  void SkipTo(const Element*, base::ReadStream&) const;

  struct Pair {
    const Element* element = nullptr;
    base::BufferReadStream stream;
  };

  struct ElementIterator {
    Pair operator*();
    Pair operator->();
    ElementIterator& operator++();

    Impl* impl_ = nullptr;
    const Element* element_ = nullptr;
    int64_t offset_ = {};
  };

  struct ElementRange {
    ElementIterator begin();
    ElementIterator end();

    Impl* impl_ = nullptr;
    std::string_view data_;
  };

  /// Iterate over all data within a single instance.
  ElementRange range(std::string_view data) const;

 private:
  std::unique_ptr<Impl> impl_;
};

}
}
