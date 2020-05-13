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

#include "mjlib/telemetry/binary_schema_parser.h"

#include <deque>

#include <fmt/format.h>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/recording_stream.h"
#include "mjlib/base/system_error.h"
#include "mjlib/telemetry/error.h"

namespace mjlib {
namespace telemetry {

using FT = Format::Type;

class BinarySchemaParser::Impl {
 public:
  Impl(std::string_view schema, std::string_view name) {
    base::BufferReadStream stream{schema};

    root_ = ReadType(nullptr, stream, name);
  }

  std::optional<Field> ReadField(
      const Element* parent, base::ReadStream& base_stream) {
    telemetry::ReadStream stream{base_stream};
    Field result;
    result.field_flags = stream.ReadVaruint().value();
    result.name = stream.ReadString().value();
    const auto naliases = stream.ReadVaruint().value();
    for (uint64_t i = 0; i < naliases; i++) {
      result.aliases.push_back(stream.ReadString().value());
    }
    result.element = ReadType(parent, stream.base(), result.name);
    const uint8_t default_present = stream.Read<uint8_t>().value();
    if (default_present == 1) {
      result.default_value = result.element->Read(stream.base());
    }

    if (result.element->type == FT::kFinal) {
      // We only return here, because even final fields still have all
      // their members and we need to consume everything.
      return {};
    }

    return result;
  }

  Element* ReadType(const Element* parent,
                    base::ReadStream& stream_in, std::string_view name) {
    base::RecordingStream recording_stream{stream_in};
    telemetry::ReadStream stream{recording_stream};

    elements_.push_back({});
    auto* const result = &elements_.back();
    result->name = name;
    result->parent = parent;

    const auto type = stream.ReadVaruint().value();
    if (type > static_cast<uint64_t>(FT::kLastType)) {
      throw base::system_error(
          {errc::kInvalidType, fmt::format("type {} unknown", type)});
    }
    result->type = static_cast<FT>(type);

    switch (result->type) {
      case FT::kFinal: {
        break;
      }
      case FT::kNull: {
        result->maybe_fixed_size = 0;
        break;
      }
      case FT::kBoolean: {
        result->maybe_fixed_size = 1;
        break;
      }
      case FT::kFloat32: {
        result->maybe_fixed_size = 4;
        break;
      }
      case FT::kFloat64: {
        result->maybe_fixed_size = 8;
        break;
      }
      case FT::kVarint:
      case FT::kVaruint:
      case FT::kBytes:
      case FT::kString: {
        // These all have no more information.
        break;
      }
      case FT::kFixedInt:
      case FT::kFixedUInt: {
        result->int_size = stream.Read<uint8_t>().value();
        result->maybe_fixed_size = result->int_size;
        break;
      }
      case FT::kObject: {
        result->object_flags = stream.ReadVaruint().value();
        int64_t maybe_size = 0;
        while (true) {
          const auto maybe_field = ReadField(result, stream.base());
          if (!maybe_field) { break; }
          result->fields.push_back(*maybe_field);
          if (maybe_field->element->maybe_fixed_size < 0) {
            maybe_size = -1;
          } else if (maybe_size >= 0) {
            maybe_size += maybe_field->element->maybe_fixed_size;
          }
        }
        if (maybe_size >= 0) {
          result->maybe_fixed_size = maybe_size;
        }
        break;
      }
      case FT::kEnum: {
        auto* const child = ReadType(result, stream.base(), name);
        result->children.push_back(child);
        const auto nvalues = stream.ReadVaruint().value();
        for (uint64_t i = 0; i < nvalues; i++) {
          const auto value = child->ReadUIntLike(stream.base());
          const auto name = stream.ReadString().value();
          result->enum_items.insert(std::make_pair(value, name));
        }
        result->maybe_fixed_size = child->maybe_fixed_size;
        break;
      }
      case FT::kFixedArray: {
        result->array_size = stream.ReadVaruint().value();
        result->children.push_back(ReadType(result, stream.base(), name));
        break;
      }
      case FT::kArray:
      case FT::kMap: {
        result->children.push_back(ReadType(result, stream.base(), name));
        break;
      }
      case FT::kUnion: {
        while (true) {
          auto* const child = ReadType(result, stream.base(), name);
          if (child->type == FT::kFinal) { break; }
          result->children.push_back(child);
        }
        break;
      }
      case FT::kTimestamp:
      case FT::kDuration: {
        result->maybe_fixed_size = 8;
        break;
      }
    }

    result->binary_schema = recording_stream.str();
    return result;
  }

  const Element* root() const {
    return root_;
  }

  ElementRange elements() const {
    return ElementRange{this};
  }

  const Element* NextElement(const Element* element) const {
    // If we have children, then we need to descend to them.
    if (element->children.size()) {
      return element->children.front();
    } else if (element->fields.size()) {
      return element->fields.front().element;
    } else {
      // We have no children, so look for a sibling progressively up
      // our parent chain.
      const Element* child = element;
      const Element* parent = element;
      while (parent->parent) {
        child = parent;
        parent = parent->parent;

        if (parent->children.size()) {
          // Find our child in this list.
          auto it = std::find(
              parent->children.begin(), parent->children.end(), child);
          MJ_ASSERT(it != parent->children.end());
          ++it;
          if (it == parent->children.end()) {
            // No siblings here.
            continue;
          }

          // We got one!
          return *it;
        } else if (parent->fields.size()) {
          auto it = std::find_if(parent->fields.begin(), parent->fields.end(),
                                 [&](const auto& field) {
                                   return field.element == child;
                                 });
          MJ_ASSERT(it != parent->fields.end());
          ++it;
          if (it == parent->fields.end()) {
            // No siblings here.
            continue;
          }

          // We got one.
          return it->element;
        }

        // No siblings at all, so just keep going up to our parent.
      }
    }

    return nullptr;
  }

 private:

  const Element* root_;

  // We use a deque, so push_back doesn't invalidate existing
  // iterators.
  std::deque<Element> elements_;
};

void BinarySchemaParser::Element::Ignore(base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};

  if (maybe_fixed_size >= 0) {
    stream.Ignore(maybe_fixed_size);
    return;
  }
  switch (type) {
    case FT::kFinal:
    case FT::kNull:
    case FT::kBoolean:
    case FT::kFixedInt:
    case FT::kFixedUInt:
    case FT::kFloat32:
    case FT::kFloat64:
    case FT::kTimestamp:
    case FT::kDuration: {
      // These all have fixed sizes.  We should not have made it here.
      base::AssertNotReached();
      break;
    }
    case FT::kVarint: {
      stream.ReadVarint();
      break;
    }
    case FT::kVaruint: {
      stream.ReadVaruint();
      break;
    }
    case FT::kBytes:
    case FT::kString: {
      const auto size = stream.ReadVaruint().value();
      stream.Ignore(size);
      break;
    }
    case FT::kObject: {
      for (const auto& field : fields) {
        field.element->Ignore(stream.base());
      }
      break;
    }
    case FT::kEnum: {
      children.front()->Ignore(stream.base());
      break;
    }
    case FT::kArray: {
      const auto nelements = stream.ReadVaruint().value();
      if (children.front()->maybe_fixed_size >= 0) {
        stream.Ignore(nelements * children.front()->maybe_fixed_size);
      } else {
        for (uint64_t i = 0; i < nelements; i++) {
          children.front()->Ignore(stream.base());
        }
      }
      break;
    }
    case FT::kFixedArray: {
      if (children.front()->maybe_fixed_size >= 0) {
        stream.Ignore(array_size * children.front()->maybe_fixed_size);
      } else {
        for (uint64_t i = 0; i < array_size; i++) {
          children.front()->Ignore(stream.base());
        }
      }
      break;
    }
    case FT::kMap: {
      const auto nitems = stream.ReadVaruint().value();
      for (uint64_t i = 0; i < nitems; i++) {
        stream.ReadString();
        children.front()->Ignore(stream.base());
      }
      break;
    }
    case FT::kUnion: {
      const auto index = stream.ReadVaruint().value();
      if (index > children.size()) {
        throw base::system_error(
            {errc::kInvalidUnionIndex,
                  fmt::format("Unknown union index {}", index)});
      }
      children[index]->Ignore(stream.base());
      break;
    }
  }
}

std::string BinarySchemaParser::Element::Read(
    base::ReadStream& base_stream) const {
  base::RecordingStream recording_stream{base_stream};
  Ignore(recording_stream);
  return recording_stream.str();
}

uint64_t BinarySchemaParser::Element::ReadArraySize(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  MJ_ASSERT(type == FT::kArray);
  return stream.ReadVaruint().value();
}

uint64_t BinarySchemaParser::Element::ReadUnionIndex(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  MJ_ASSERT(type == FT::kUnion);
  return stream.ReadVaruint().value();
}

bool BinarySchemaParser::Element::ReadBoolean(
    base::ReadStream& base_stream) const {
  MJ_ASSERT(type == FT::kBoolean);
  return telemetry::ReadStream(base_stream).Read<uint8_t>().value() != 0;
}

uint64_t BinarySchemaParser::Element::ReadUIntLike(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  switch (type) {
    case FT::kFixedUInt: {
      switch (int_size) {
        case 1: {
          return stream.Read<uint8_t>().value();
        }
        case 2: {
          return stream.Read<uint16_t>().value();
        }
        case 4: {
          return stream.Read<uint32_t>().value();
        }
        case 8: {
          return stream.Read<uint64_t>().value();
        }
      }
      base::AssertNotReached();
    }
    case FT::kVaruint: {
      return stream.ReadVaruint().value();
    }
    case FT::kEnum: {
      return children.front()->ReadUIntLike(base_stream);
    }
    default: {
      base::AssertNotReached();
    }
  }
}

int64_t BinarySchemaParser::Element::ReadIntLike(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  switch (type) {
    case FT::kFixedInt: {
      switch (int_size) {
        case 1: {
          return stream.Read<int8_t>().value();
        }
        case 2: {
          return stream.Read<int16_t>().value();
        }
        case 4: {
          return stream.Read<int32_t>().value();
        }
        case 8: {
          return stream.Read<int64_t>().value();
        }
      }
      base::AssertNotReached();
    }
    case FT::kVarint: {
      return stream.ReadVarint().value();
    }
    case FT::kTimestamp:
    case FT::kDuration: {
      return stream.Read<int64_t>().value();
    }
    default: {
      base::AssertNotReached();
    }
  }
}

double BinarySchemaParser::Element::ReadFloatLike(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  switch (type) {
    case FT::kFloat32: {
      return stream.Read<float>().value();
    }
    case FT::kFloat64: {
      return stream.Read<double>().value();
    }
    default: {
      base::AssertNotReached();
    }
  }
}

std::string BinarySchemaParser::Element::ReadString(
    base::ReadStream& base_stream) const {
  telemetry::ReadStream stream{base_stream};
  switch (type) {
    case FT::kBytes:
    case FT::kString: {
      return stream.ReadString().value();
    }
    default: {
      base::AssertNotReached();
    }
  }
}

BinarySchemaParser::BinarySchemaParser(std::string_view schema,
                                       std::string_view record_name)
    : impl_(std::make_unique<Impl>(schema, record_name)) {}

BinarySchemaParser::~BinarySchemaParser() {}

const BinarySchemaParser::Element* BinarySchemaParser::root() const {
  return impl_->root();
}

BinarySchemaParser::ElementRange BinarySchemaParser::elements() const {
  return impl_->elements();
}

BinarySchemaParser::ElementIterator&
BinarySchemaParser::ElementIterator::operator++() {
  element_ = impl_->NextElement(element_);
  if (element_ == nullptr) {
    impl_ = nullptr;
  }
  return *this;
}

bool BinarySchemaParser::ElementIterator::operator!=(
    const ElementIterator& rhs) const {
  return impl_ != rhs.impl_ || element_ != rhs.element_;
}

BinarySchemaParser::ElementIterator
BinarySchemaParser::ElementRange::begin() const {
  return ElementIterator(impl_, impl_->root());
}

BinarySchemaParser::ElementIterator
BinarySchemaParser::ElementRange::end() const {
  return ElementIterator(nullptr, nullptr);
}

}
}
