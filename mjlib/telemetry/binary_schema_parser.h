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

#include <map>
#include <string>
#include <vector>

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
    uint64_t field_flags = 0;
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
    std::string binary_schema;

    /// If known, the fixed offset within a data record of this
    /// element.
    int64_t maybe_fixed_offset = -1;

    /// If known, the fixed size of this data element and all
    /// children.
    int64_t maybe_fixed_size = -1;

    /////////
    /// Available for some types
    std::vector<std::string> aliases;

    /// for kFixedInt and kFixedUInt
    int int_size = -1;

    /// For kArray, kMap, kUnion, kEnum, and kFixedArray
    std::vector<const Element*> children;

    /// For kFixedArray
    uint64_t array_size = 0;

    /// For kEnum
    std::map<uint64_t, std::string> enum_items;

    /// For kObject
    std::vector<Field> fields;
    uint64_t object_flags = 0;

    /////////
    /// All read methods require the stream be properly positioned.
    /// It is a programmatic error to call a read method for an
    /// incorrect type.

    /// Ignore the entire data contents of this element.
    void Ignore(base::ReadStream&) const;

    /// Read the entire data contents of this element.
    std::string Read(base::ReadStream&) const;

    uint64_t ReadArraySize(base::ReadStream&) const;
    uint64_t ReadUnionIndex(base::ReadStream&) const;
    bool ReadBoolean(base::ReadStream&) const;
    uint64_t ReadUIntLike(base::ReadStream&) const;
    int64_t ReadIntLike(base::ReadStream&) const;
    double ReadFloatLike(base::ReadStream&) const;
    std::string ReadString(base::ReadStream&) const;
  };

  const Element* root() const;

  struct ElementIterator {
    ElementIterator(const Impl* impl, const Element* element)
        : impl_(impl), element_(element) {}

    const Element& operator*() const { return *element_; }
    const Element* operator->() const { return element_; }

    ElementIterator& operator++();

    bool operator!=(const ElementIterator&) const;

   private:
    const Impl* impl_ = nullptr;
    const Element* element_;
  };

  struct ElementRange {
    ElementRange(const Impl* impl) : impl_(impl) {}

    ElementIterator begin() const;
    ElementIterator end() const;

   private:
    const Impl* impl_ = nullptr;
  };

  /// Iterate over all schema elements.  Each container is visited
  /// before its children.  Children of containers are visited exactly
  /// once, as there is no data yet to determine a number of elements.
  ElementRange elements() const;

 private:
  std::unique_ptr<Impl> impl_;
};

}
}
