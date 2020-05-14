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

#include "mjlib/base/detail/serialize.h"
#include "mjlib/base/error_code.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/priority_tag.h"
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
  using StringMap = std::map<std::string, std::string>;

  MappedBinaryReader(const BinarySchemaParser* parser)
      : MappedBinaryReader(parser->root()) {}

  MappedBinaryReader(const Element* element)
      : element_(element) {
    if (element->binary_schema == BinarySchemaArchive::Write<ParentType>()) {
      schemas_same_ = true;
    } else if (base::IsSerializable<ParentType>() &&
               element->type == Format::Type::kObject) {
      // We can map on a field level, prepare our mapping structure.
      for (const auto& field : element->fields) {
        element_map_.insert(std::make_pair(field.name, field.element));
      }

      SetupReaders<base::IsNativeSerializable<ParentType>() ? 1 :
                   base::IsExternalSerializable<ParentType>() ? 2 : 0>();
    } else {
      throw base::system_error(
          base::error_code(
              errc::kTypeMismatch,
              fmt::format(
                  "C++ {} has incorrect type for {}",
                  typeid(ParentType).name(),
                  static_cast<int>(element->type))));
    }
  }

  template <int _>
  void SetupReaders() {
    // We have this dummy method so that we don't instantiate things
    // for non-structure types.
    base::AssertNotReached();
  }

  template <>
  void SetupReaders<1>() {
    ParentType parent;
    SetupNativeSerializable<ParentType>(&parent);
  }

  template <typename Serializable>
  void SetupNativeSerializable(Serializable* serializable) {
    SchemaArchive archive(&element_map_, this);
    base::Serialize(serializable, &archive);
  }

  // NOTE: This class would be very simple if we didn't have to deal
  // with external serialization, which adds a lot of complexity.  We
  // have to handle external serialization of types that resolve to
  // primitives and those that resolve to serializable, all while
  // making we we don't instantiate a template that doesn't make
  // sense.

  struct SetupReaderExternal {
    SetupReaderExternal(MappedBinaryReader* parent) : parent_(parent) {}

    template <typename T>
    void operator()(const T& nvp) {
      using ChildRef = decltype(*nvp.value());
      using Child = typename std::remove_reference<ChildRef>::type;
      SetupReaderExternalHelper<
        base::IsNativeSerializable<Child>() ? 1 : 0>{parent_}(nvp.value());
    }

    MappedBinaryReader* parent_;
  };

  template <int _>
  struct SetupReaderExternalHelper {
    SetupReaderExternalHelper(MappedBinaryReader*) {}

    template <typename Serializable>
    void operator()(Serializable*) {}
  };

  template <>
  struct SetupReaderExternalHelper<1> {
    SetupReaderExternalHelper(MappedBinaryReader* parent) : parent_(parent) {}

    template <typename Serializable>
    void operator()(Serializable* serializable) {
      parent_->SetupNativeSerializable<Serializable>(serializable);
    }

    MappedBinaryReader* parent_;
  };

  template <>
  void SetupReaders<2>() {
    // This only works if the type we get back from the external
    // serializer is also an object.
    SchemaArchive archive(&element_map_, this);
    ParentType value;
    mjlib::base::ExternalSerializer<ParentType> serializer;
    serializer.Serialize(&value, SetupReaderExternal(this));
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

      ReadHelper<base::IsNativeSerializable<ParentType>() ? 1 :
                 base::IsExternalSerializable<ParentType>() ? 2 : 0>(
                     &field_data, value);
    }
  }

  template <int _>
  void ReadHelper(StringMap*, ParentType*) const {
    base::AssertNotReached();
  }

  template <>
  void ReadHelper<1>(StringMap* field_data, ParentType* value) const {
    // Now visit the C++ structure.
    DataArchive archive(field_data, &readers_);
    base::Serialize(value, &archive);
  }

  template <>
  void ReadHelper<2>(StringMap* field_data, ParentType* value) const {
    mjlib::base::ExternalSerializer<ParentType> serializer;
    serializer.Serialize(value, ExternalDataHelper{field_data, &readers_});
  }

 private:
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
      VisitHelper(nvp, nvp.value(), base::PriorityTag<1>());
    }

    template <typename NameValuePair, typename T>
    void VisitHelper(const NameValuePair& nvp, T* value, base::PriorityTag<1>,
                     std::enable_if_t<base::IsExternalSerializable<T>(), int> = 0) {
      // We have to invoke the external serializer here.
      mjlib::base::ExternalSerializer<T> serializer;
      serializer.Serialize(value, [&](const auto& new_nvp) {
          base::detail::NameValuePairNameOverride old_name(new_nvp, nvp.name());
          Visit(old_name);
        });
    }

    template <typename NameValuePair, typename T>
    void VisitHelper(const NameValuePair& nvp, T* value, base::PriorityTag<0>) {
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

  template <int _>
  struct ExtraHelper {
    template <typename NameValuePair>
    void operator()(DataArchive* archive, const NameValuePair& nvp) {
      archive->Visit(nvp);
    }
  };

  template<>
  struct ExtraHelper<1> {
    template <typename NameValuePair>
    void operator()(DataArchive* archive, const NameValuePair& nvp) {
      nvp.value()->Serialize(archive);
    }
  };

  struct ExternalDataHelper {
    ExternalDataHelper(StringMap* field_data_in, const ReaderMap* readers_in)
        : archive(field_data_in, readers_in) {}

    template <typename NameValuePair>
    void operator()(const NameValuePair& nvp) {
      using ChildRef = decltype(*nvp.value());
      using Child = typename std::remove_reference<ChildRef>::type;
      ExtraHelper<
        base::IsNativeSerializable<Child>() ? 1 : 0>{}(&archive, nvp);
    }

    DataArchive archive;
  };

  bool schemas_same_ = false;
  const Element* element_;

  // There is one entry here for each field of the C++ structure,
  // indexed by name.
  ReaderMap readers_;

  // This is really just a member variable for convenience.
  StringElementMap element_map_;
};

}
}
