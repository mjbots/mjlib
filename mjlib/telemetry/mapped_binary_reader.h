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

#include <memory>
#include <string>

#include <fmt/format.h>

#include "mjlib/base/error_code.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/recording_stream.h"
#include "mjlib/base/system_error.h"

#include "mjlib/telemetry/binary_read_archive.h"
#include "mjlib/telemetry/binary_schema_parser.h"
#include "mjlib/telemetry/binary_write_archive.h"
#include "mjlib/telemetry/error.h"

namespace mjlib {
namespace telemetry {

/// Given a binary schema, provide mechanisms to read data matching
/// that schema into a C++ structure that may differ.  Differences are
/// handled according to the schema evolution rules documented in
/// README.md.
template <class ParentType>
class MappedBinaryReader {
 public:
  using Element = BinarySchemaParser::Element;
  using StringElementMap = std::map<std::string, const Element*>;

  MappedBinaryReader(const BinarySchemaParser* parser)
      : MappedBinaryReader(parser->root()) {}

  MappedBinaryReader(const Element* element)
      : element_(element) {
    if (element->binary_schema == BinarySchemaArchive::Write<ParentType>()) {
      schemas_same_ = true;
    } else if (base::IsSerializable<ParentType>() &&
               element->type == Format::Type::kObject) {
      SetupReaders<base::IsSerializable<ParentType>()>(element);
    } else {
      throw base::system_error(
          base::error_code(
              errc::kTypeMismatch,
              fmt::format(
                  "C++ {} has incorrect type for {}/{}",
                  typeid(ParentType).name(),
                  element->name, static_cast<int>(element->type))));
    }
  }

  template <bool _>
  void SetupReaders(const Element*) {
    // We have this dummy method so that we don't instantiate things
    // for non-structure types.
    base::AssertNotReached();
  }

  template <>
  void SetupReaders<true>(const Element* element) {
      // We can map on a field level, prepare our mapping structure.
      StringElementMap element_map;
      for (const auto& field : element->fields) {
        element_map.insert(std::make_pair(field.name, field.element));
      }
      SchemaArchive archive(&element_map, this);
      ParentType value;
      base::Serialize(&value, &archive);
  }

  ParentType Read(std::string_view data) const {
    ParentType result;
    Read(&result, data);
    return result;
  }

  void Read(ParentType* value, std::string_view data) const {
    base::BufferReadStream stream(data);
    Read(value, stream);
  }

  void Read(ParentType* value, base::ReadStream& stream) const {
    if (schemas_same_) {
      BinaryReadArchive(stream).Value(value);
      return;
    } else {
      ReadHelper<base::IsSerializable<ParentType>()>(value, stream);
    }
  }

  template <bool _>
  void ReadHelper(ParentType*, base::ReadStream&) const {
    base::AssertNotReached();
  }

  template <>
  void ReadHelper<true>(ParentType* value, base::ReadStream& stream) const {
    // Read all the field data in an unstructured manner.
    //
    // NOTE: You could imagine delaying this, and reading fields
    // directly until one was found which needed the mapping at which
    // point some or all of the remainder would be read into temporary
    // memory.
    StringMap field_data;
    for (const auto& field : element_->fields) {
      base::RecordingStream recording_stream{stream};
      field.element->Ignore(recording_stream);
      field_data.insert(std::make_pair(field.name, recording_stream.str()));
    }

    // Now visit the C++ structure.
    DataArchive archive(&field_data, &readers_);
    base::Serialize(value, &archive);
  }

 private:
  using StringMap = std::map<std::string, std::string>;

  class Reader {
   public:
    virtual ~Reader() {}

    virtual void Read(std::string_view data, void* value) = 0;
  };

  using ReaderMap = std::map<std::string, std::unique_ptr<Reader>>;

  template <typename ChildType>
  class ChildReader : public Reader {
   public:
    ChildReader(const Element* element) : reader_(element) {}
    ~ChildReader() override {}

    void Read(std::string_view data, void* value) override {
      ChildType* child = reinterpret_cast<ChildType*>(value);
      reader_.Read(child, data);
    }

    MappedBinaryReader<ChildType> reader_;
  };

  class SchemaArchive {
   public:
    SchemaArchive(const StringElementMap* string_element_map,
                  MappedBinaryReader* parent)
        : string_element_map_(string_element_map),
          parent_(parent) {}

    template <typename NameValuePair>
    void Visit(const NameValuePair& nvp) {
      using ChildTypeCV = decltype(*nvp.value());
      using ChildType = typename std::remove_const<
        typename std::remove_reference<ChildTypeCV>::type>::type;
      auto it = string_element_map_->find(nvp.name());
      if (it == string_element_map_->end()) { return; }

      parent_->readers_.insert(
          std::make_pair(nvp.name(),
                         std::make_unique<ChildReader<ChildType>>(
                             it->second)));
    }

   private:
    const StringElementMap* const string_element_map_;
    MappedBinaryReader* const parent_;
  };

  class DataArchive {
   public:
    DataArchive(const StringMap* data, const ReaderMap* reader_map)
        : data_(data), reader_map_(reader_map) {}

    template <typename NameValuePair>
    void Visit(const NameValuePair& nvp) {
      const auto rit = reader_map_->find(nvp.name());
      if (rit == reader_map_->end()) { return; }

      const auto dit = data_->find(nvp.name());
      if (dit == data_->end()) { return; }

      rit->second->Read(dit->second, nvp.value());
    }

   private:
    const StringMap* const data_;
    const ReaderMap* const reader_map_;
  };

  bool schemas_same_ = false;
  const Element* element_;

  // There is one entry here for each field of the C++ structure,
  // indexed by name.
  ReaderMap readers_;
};

}
}
