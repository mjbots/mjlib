// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/micro/serializable_handler.h"

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mjlib/base/tokenizer.h"

#include "mjlib/telemetry/format.h"

/// @file
///
/// This interpreter must produce results identical to the templated
/// archives it replaces: mjlib::telemetry::BinaryWriteArchive,
/// BinarySchemaArchive, and BinaryReadArchive, along with the text
/// archives now found in serializable_handler_legacy.h.  In
/// particular, the schema must be byte-for-byte identical, as its CRC
/// is used to validate configuration stored in flash.

namespace mjlib {
namespace micro {

void EmitField(FieldVisitor& visitor, const char* name, void* value,
               const TypeDescriptor* type) {
  visitor.Field(name, value, type);
}

namespace {
using TF = telemetry::Format;

template <typename T>
T Load(const void* ptr) {
  T result;
  std::memcpy(&result, ptr, sizeof(result));
  return result;
}

template <typename T>
void Store(void* ptr, T value) {
  std::memcpy(ptr, &value, sizeof(value));
}

const EnumDescriptor* AsEnum(const TypeDescriptor* type) {
  return static_cast<const EnumDescriptor*>(type);
}

const ArrayDescriptor* AsArray(const TypeDescriptor* type) {
  return static_cast<const ArrayDescriptor*>(type);
}

const OptionalDescriptor* AsOptional(const TypeDescriptor* type) {
  return static_cast<const OptionalDescriptor*>(type);
}

void* ArrayElement(void* array, const ArrayDescriptor* desc, int index) {
  return static_cast<char*>(array) + index * desc->stride;
}

constexpr uint8_t kScalarSizes[] = {1, 1, 2, 4, 8, 1, 2, 4, 8, 4, 8};
static_assert(sizeof(kScalarSizes) ==
              static_cast<int>(TypeKind::kFloat64) + 1);

int ScalarSize(TypeKind kind) {
  return kind <= TypeKind::kFloat64 ?
      kScalarSizes[static_cast<int>(kind)] : 0;
}

bool IsSigned(TypeKind kind) {
  return kind == TypeKind::kInt8 || kind == TypeKind::kInt16 ||
      kind == TypeKind::kInt32 || kind == TypeKind::kInt64;
}

/// Load an integer of the given kind, sign or zero extending as
/// appropriate.
int64_t LoadInteger(const void* ptr, TypeKind kind) {
  switch (kind) {
    case TypeKind::kInt8: { return Load<int8_t>(ptr); }
    case TypeKind::kInt16: { return Load<int16_t>(ptr); }
    case TypeKind::kInt32: { return Load<int32_t>(ptr); }
    case TypeKind::kInt64: { return Load<int64_t>(ptr); }
    case TypeKind::kBool:
    case TypeKind::kUInt8: { return Load<uint8_t>(ptr); }
    case TypeKind::kUInt16: { return Load<uint16_t>(ptr); }
    case TypeKind::kUInt32: { return Load<uint32_t>(ptr); }
    case TypeKind::kUInt64: {
      return static_cast<int64_t>(Load<uint64_t>(ptr));
    }
    default: {
      return 0;
    }
  }
}

/// Store an integer, truncating to the size of the given kind.
void StoreInteger(void* ptr, TypeKind kind, uint64_t value) {
  switch (ScalarSize(kind)) {
    case 1: { Store(ptr, static_cast<uint8_t>(value)); break; }
    case 2: { Store(ptr, static_cast<uint16_t>(value)); break; }
    case 4: { Store(ptr, static_cast<uint32_t>(value)); break; }
    case 8: { Store(ptr, value); break; }
  }
}

void VisitStructFields(void* object, const TypeDescriptor* type,
                       FieldVisitor& visitor) {
  static_cast<const StructDescriptor*>(type)->visit_fields(object, visitor);
}

template <typename Functor>
class FieldFunctor final : public FieldVisitor {
 public:
  explicit FieldFunctor(Functor& functor) : functor_(functor) {}

  void Field(const char* name, void* value,
             const TypeDescriptor* type) override {
    functor_(name, value, type);
  }

 private:
  Functor& functor_;
};

template <typename Functor>
void ForEachField(void* object, const TypeDescriptor* type,
                  Functor functor) {
  FieldFunctor<Functor> visitor(functor);
  VisitStructFields(object, type, visitor);
}

template <typename Functor>
class ValueFunctor final : public ValueVisitor {
 public:
  explicit ValueFunctor(Functor& functor) : functor_(functor) {}

  void Value(void* value, const TypeDescriptor* type) override {
    functor_(value, type);
  }

 private:
  Functor& functor_;
};

/// Invoke @p functor with the value an externally serialized type
/// maps to.
template <typename Functor>
void Unwrap(void* object, const TypeDescriptor* type, Functor functor) {
  ValueFunctor<Functor> visitor(functor);
  static_cast<const ExternalDescriptor*>(type)->visit(object, visitor);
}

/////////////////////////////////////
// Binary data

/// Writes the binary encoding of values.
///
/// The encoding of a scalar is identical to its little endian
/// representation in memory.  So, consecutive scalars which are also
/// adjacent in memory (most fields of most structures) are coalesced
/// into a single write to the underlying stream.
class BinaryWriter final : public FieldVisitor {
 public:
  explicit BinaryWriter(telemetry::WriteStream& stream) : stream_(stream) {}

  ~BinaryWriter() { Flush(); }

  void Field(const char*, void* value, const TypeDescriptor* type) override {
    // The common case, a scalar which immediately follows the current
    // run, is handled here without needing a stack frame.
    const auto kind = type->kind;
    const char* const ptr = static_cast<const char*>(value);
    if (kind <= TypeKind::kFloat64 &&
        (kind != TypeKind::kBool || Load<uint8_t>(ptr) <= 1)) {
      if (ptr != start_ + size_) {
        Flush();
        start_ = ptr;
      }
      size_ += kScalarSizes[static_cast<int>(kind)];
      return;
    }
    Value(value, type);
  }

  __attribute__((noinline))
  void Value(void* value, const TypeDescriptor* type) {
    const auto kind = type->kind;
    if (kind <= TypeKind::kFloat64) {
      // A bool must be written as exactly 0 or 1.
      if (kind == TypeKind::kBool && Load<uint8_t>(value) > 1) {
        Flush();
        stream_.Write(true);
        return;
      }
      Append(value, ScalarSize(kind));
      return;
    }

    switch (kind) {
      case TypeKind::kEnum: {
        Flush();
        stream_.WriteVaruint(static_cast<uint64_t>(
                                 LoadInteger(value, AsEnum(type)->underlying)));
        return;
      }
      case TypeKind::kArray: {
        const auto* desc = AsArray(type);
        const auto element_kind = desc->element->kind;
        if (element_kind <= TypeKind::kFloat64 &&
            element_kind != TypeKind::kBool) {
          // The elements are contiguous.
          Append(value, desc->size * desc->stride);
          return;
        }
        for (int i = 0; i < desc->size; i++) {
          Value(ArrayElement(value, desc, i), desc->element);
        }
        return;
      }
      case TypeKind::kOptional: {
        const auto* desc = AsOptional(type);
        void* const contained = desc->get(value);
        Flush();
        stream_.WriteVaruint(contained ? 1 : 0);
        if (contained) { Value(contained, desc->element); }
        return;
      }
      case TypeKind::kStruct: {
        static_cast<const StructDescriptor*>(type)->visit_fields(value, *this);
        return;
      }
      case TypeKind::kExternal: {
        Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
            Value(inner, inner_type);
          });
        return;
      }
      default: {
        return;
      }
    }
  }

  __attribute__((noinline))
  void Flush() {
    if (size_) {
      stream_.RawWrite({start_, size_});
      size_ = 0;
    }
  }

 private:
  void Append(const void* data, std::size_t size) {
    const char* const ptr = static_cast<const char*>(data);
    if (ptr != start_ + size_) {
      Flush();
      start_ = ptr;
    }
    size_ += size;
  }

  telemetry::WriteStream& stream_;
  const char* start_ = nullptr;
  std::size_t size_ = 0;
};

void WriteValue(telemetry::WriteStream& stream, void* value,
                const TypeDescriptor* type) {
  BinaryWriter writer(stream);
  writer.Value(value, type);
}

/// Reads the binary encoding of values.
class BinaryReader final : public FieldVisitor {
 public:
  explicit BinaryReader(telemetry::ReadStream& stream) : stream_(stream) {}

  bool error() const { return error_; }

  void Field(const char*, void* value, const TypeDescriptor* type) override {
    const auto kind = type->kind;
    if (kind <= TypeKind::kFloat64) {
      Scalar(value, kind);
      return;
    }
    Value(value, type);
  }

  __attribute__((noinline))
  void Value(void* value, const TypeDescriptor* type) {
    switch (type->kind) {
      case TypeKind::kEnum: {
        const auto maybe_value = stream_.ReadVaruint();
        if (!maybe_value) {
          error_ = true;
          return;
        }
        StoreInteger(value, AsEnum(type)->underlying, *maybe_value);
        return;
      }
      case TypeKind::kArray: {
        const auto* desc = AsArray(type);
        // As the original implementation did, stop after the first
        // element which fails, even if an error happened earlier.
        for (int i = 0; i < desc->size; i++) {
          Field(nullptr, ArrayElement(value, desc, i), desc->element);
          if (error_) { return; }
        }
        return;
      }
      case TypeKind::kOptional: {
        const auto maybe_present = stream_.Read<uint8_t>();
        if (!maybe_present) {
          error_ = true;
          return;
        }
        const auto present = *maybe_present;
        if (present == 0) {
          // nothing
        } else if (present == 1) {
          const auto* desc = AsOptional(type);
          Field(nullptr, desc->emplace(value), desc->element);
        } else {
          error_ = true;
        }
        return;
      }
      case TypeKind::kStruct: {
        static_cast<const StructDescriptor*>(type)->visit_fields(value, *this);
        return;
      }
      case TypeKind::kExternal: {
        Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
            Field(nullptr, inner, inner_type);
          });
        return;
      }
      default: {
        Scalar(value, type->kind);
        return;
      }
    }
  }

 private:
  void Scalar(void* value, TypeKind kind) {
    // Only modify the destination if the read succeeds.
    char temp[8];
    const int size = kScalarSizes[static_cast<int>(kind)];
    auto& base = stream_.base();
    base.read({temp, size});
    if (base.gcount() != size) {
      error_ = true;
      return;
    }
    switch (size) {
      case 1: { Store(value, Load<uint8_t>(temp)); break; }
      case 2: { Store(value, Load<uint16_t>(temp)); break; }
      case 4: { Store(value, Load<uint32_t>(temp)); break; }
      default: { Store(value, Load<uint64_t>(temp)); break; }
    }
  }

  telemetry::ReadStream& stream_;
  bool error_ = false;
};

/////////////////////////////////////
// Binary schema


void WriteSchemaType(telemetry::WriteStream&, void* value,
                     const TypeDescriptor*);

/// Scalar fields, the most common, have their entire schema record
/// assembled and written at once.
///
/// @return false if this was not possible
bool WriteScalarFieldSchema(telemetry::WriteStream& stream,
                            const char* name,
                            void* value,
                            TypeKind kind) {
  // flags, name size, name, naliases, type (2), has default, value (8)
  char buffer[80];
  const std::size_t name_size = std::strlen(name);
  if (name_size >= 0x80 || name_size + 14 > sizeof(buffer)) {
    return false;
  }

  char* it = buffer;
  *it++ = 0;  // FieldFlags
  *it++ = static_cast<char>(name_size);
  std::memcpy(it, name, name_size);
  it += name_size;
  *it++ = 0;  // naliases

  const int size = kScalarSizes[static_cast<int>(kind)];
  if (kind == TypeKind::kBool) {
    *it++ = static_cast<char>(TF::Type::kBoolean);
  } else if (kind <= TypeKind::kInt64) {
    *it++ = static_cast<char>(TF::Type::kFixedInt);
    *it++ = static_cast<char>(size);
  } else if (kind <= TypeKind::kUInt64) {
    *it++ = static_cast<char>(TF::Type::kFixedUInt);
    *it++ = static_cast<char>(size);
  } else if (kind == TypeKind::kFloat32) {
    *it++ = static_cast<char>(TF::Type::kFloat32);
  } else {
    *it++ = static_cast<char>(TF::Type::kFloat64);
  }

  *it++ = 1;  // default value has data
  if (kind == TypeKind::kBool) {
    *it++ = Load<uint8_t>(value) != 0 ? 1 : 0;
  } else {
    std::memcpy(it, value, size);
    it += size;
  }

  stream.RawWrite({buffer, static_cast<std::size_t>(it - buffer)});
  return true;
}

void WriteFieldSchema(telemetry::WriteStream& stream,
                      const char* name,
                      void* value,
                      const TypeDescriptor* type) {
  if (type->kind <= TypeKind::kFloat64 &&
      WriteScalarFieldSchema(stream, name, value, type->kind)) {
    return;
  }

  stream.WriteVaruint(0);  // FieldFlags
  stream.WriteString(name);
  stream.WriteVaruint(0);  // naliases

  WriteSchemaType(stream, value, type);

  stream.WriteVaruint(1);  // default value has data
  WriteValue(stream, value, type);
}

struct ElementSchemaContext {
  telemetry::WriteStream* stream;
  const TypeDescriptor* element;
};

/// Scalar and enumeration elements only contribute their type, so
/// they need no value.  All others are given a default constructed
/// temporary, as their fields have default values.
void WriteElementSchema(telemetry::WriteStream& stream,
                        const TypeDescriptor* element,
                        WithDefaultFunction element_default) {
  if (!element_default) {
    WriteSchemaType(stream, nullptr, element);
    return;
  }
  ElementSchemaContext context{&stream, element};
  element_default([](void* context_ptr, void* object) {
      auto* ctx = static_cast<ElementSchemaContext*>(context_ptr);
      WriteSchemaType(*ctx->stream, object, ctx->element);
    }, &context);
}

namespace writer_detail {
void WriteEnumEntry(telemetry::WriteStream& stream,
                    int64_t value, const char* name) {
  stream.WriteVaruint(value);
  stream.WriteString(name);
}
}

class EnumSchemaWriter final : public EnumEntryVisitor {
 public:
  explicit EnumSchemaWriter(telemetry::WriteStream& stream) : stream_(stream) {}

  void Entry(int64_t value, const char* name) override {
    writer_detail::WriteEnumEntry(stream_, value, name);
  }

 private:
  telemetry::WriteStream& stream_;
};

void WriteSchemaType(telemetry::WriteStream& stream, void* value,
                     const TypeDescriptor* type) {
  switch (type->kind) {
    case TypeKind::kBool: {
      stream.WriteVaruint(TF::Type::kBoolean);
      return;
    }
    case TypeKind::kInt8:
    case TypeKind::kInt16:
    case TypeKind::kInt32:
    case TypeKind::kInt64: {
      stream.WriteVaruint(TF::Type::kFixedInt);
      stream.WriteVaruint(ScalarSize(type->kind));
      return;
    }
    case TypeKind::kUInt8:
    case TypeKind::kUInt16:
    case TypeKind::kUInt32:
    case TypeKind::kUInt64: {
      stream.WriteVaruint(TF::Type::kFixedUInt);
      stream.WriteVaruint(ScalarSize(type->kind));
      return;
    }
    case TypeKind::kFloat32: {
      stream.WriteVaruint(TF::Type::kFloat32);
      return;
    }
    case TypeKind::kFloat64: {
      stream.WriteVaruint(TF::Type::kFloat64);
      return;
    }
    case TypeKind::kEnum: {
      const auto* desc = AsEnum(type);
      stream.WriteVaruint(TF::Type::kEnum);
      // For now, we only provide a way to have enums of type varuint.
      stream.WriteVaruint(TF::Type::kVaruint);
      stream.WriteVaruint(desc->count);
      if (desc->visit_entries) {
        EnumSchemaWriter writer(stream);
        desc->visit_entries(writer);
      } else {
        const bool is_signed = IsSigned(desc->underlying);
        for (uint16_t i = 0; i < desc->count; i++) {
          const auto& entry = desc->entries[i];
          const int64_t value =
              is_signed ?
              static_cast<int64_t>(static_cast<int32_t>(entry.value)) :
              static_cast<int64_t>(entry.value);
          writer_detail::WriteEnumEntry(stream, value, entry.name);
        }
      }
      return;
    }
    case TypeKind::kArray: {
      const auto* desc = AsArray(type);
      stream.WriteVaruint(TF::Type::kFixedArray);
      stream.WriteVaruint(desc->size);
      WriteElementSchema(stream, desc->element, desc->element_default);
      return;
    }
    case TypeKind::kOptional: {
      // Emit this as a union of null and the actual type.
      const auto* desc = AsOptional(type);
      stream.WriteVaruint(TF::Type::kUnion);
      stream.WriteVaruint(TF::Type::kNull);
      WriteElementSchema(stream, desc->element, desc->element_default);
      stream.WriteVaruint(TF::Type::kFinal);
      return;
    }
    case TypeKind::kStruct: {
      stream.WriteVaruint(TF::Type::kObject);
      stream.WriteVaruint(0);  // ObjectFlags
      ForEachField(value, type, [&](const char* name, void* field_value,
                                    const TypeDescriptor* field_type) {
                     WriteFieldSchema(stream, name, field_value, field_type);
                   });

      // Write out the "final" record.
      stream.WriteVaruint(0);  // FieldFlags
      stream.WriteString("");
      stream.WriteVaruint(0);  // naliases
      stream.WriteVaruint(0);  // kFinal
      stream.WriteVaruint(0);  // default value: null
      return;
    }
    case TypeKind::kExternal: {
      Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
          WriteSchemaType(stream, inner, inner_type);
        });
      return;
    }
  }
}

/////////////////////////////////////
// Text

/// snprintf a scalar or enumeration in the text format.  A null
/// value represents an empty optional, which formats as nothing.
int FormatText(char* buffer, std::size_t size,
               const void* value, const TypeDescriptor* type) {
  if (value == nullptr) {
    if (size) { buffer[0] = 0; }
    return 0;
  }

  switch (type->kind) {
    case TypeKind::kBool: {
      // Loaded as a byte, as corrupt input could leave a bool with a
      // value other than 0 or 1.
      return ::snprintf(buffer, size, "%" PRIu8, Load<uint8_t>(value));
    }
    case TypeKind::kInt8: {
      return ::snprintf(buffer, size, "%" PRIi8, Load<int8_t>(value));
    }
    case TypeKind::kInt16: {
      return ::snprintf(buffer, size, "%" PRIi16, Load<int16_t>(value));
    }
    case TypeKind::kInt32: {
      return ::snprintf(buffer, size, "%" PRIi32, Load<int32_t>(value));
    }
    case TypeKind::kInt64: {
      return ::snprintf(buffer, size, "%" PRIi64, Load<int64_t>(value));
    }
    case TypeKind::kUInt8: {
      return ::snprintf(buffer, size, "%" PRIu8, Load<uint8_t>(value));
    }
    case TypeKind::kUInt16: {
      return ::snprintf(buffer, size, "%" PRIu16, Load<uint16_t>(value));
    }
    case TypeKind::kUInt32: {
      return ::snprintf(buffer, size, "%" PRIu32, Load<uint32_t>(value));
    }
    case TypeKind::kUInt64: {
      return ::snprintf(buffer, size, "%" PRIu64, Load<uint64_t>(value));
    }
    case TypeKind::kFloat32: {
      return ::snprintf(buffer, size, "%g",
                        static_cast<double>(Load<float>(value)));
    }
    case TypeKind::kFloat64: {
      return ::snprintf(buffer, size, "%g", Load<double>(value));
    }
    case TypeKind::kEnum: {
      return ::snprintf(
          buffer, size, "%" PRIi32,
          static_cast<int32_t>(
              LoadInteger(value, AsEnum(type)->underlying)));
    }
    default: {
      break;
    }
  }
  if (size) { buffer[0] = 0; }
  return 0;
}

/// Invoked with the scalar or enumeration found by Locate.  A null
/// value indicates an empty optional.
class LeafAction {
 public:
  virtual void Apply(void* value, const TypeDescriptor* type) = 0;

 protected:
  ~LeafAction() {}
};

/// Parse an array index, returning -1 if it is malformed or out of
/// range.  This is kept out of line so that its buffer is not part of
/// every level of recursion in Locate.
__attribute__((noinline))
int ParseIndex(std::string_view index_str, int size) {
  char index_buf[32] = {};
  if (index_str.empty() || index_str.size() >= sizeof(index_buf)) {
    return -1;
  }
  std::memcpy(index_buf, index_str.data(), index_str.size());
  char* str_end = nullptr;
  const long index = std::strtol(index_buf, &str_end, 0);
  if (index < 0 ||
      str_end != index_buf + index_str.size() ||
      index >= size) {
    return -1;
  }
  return static_cast<int>(index);
}

/// Find the scalar identified by the dot separated @p key.
///
/// Any unused portion of the key after a scalar is found is ignored.
/// If @p create is true, empty optionals are filled in along the way.
///
/// @return true if found
bool Locate(void* value, const TypeDescriptor* type,
            std::string_view key, bool create, LeafAction& action) {
  switch (type->kind) {
    case TypeKind::kStruct: {
      base::Tokenizer tokenizer(key, ".");
      const auto my_key = tokenizer.next();
      const auto remaining_key = tokenizer.remaining();
      bool done = false;
      bool found = false;
      ForEachField(value, type, [&](const char* name, void* field_value,
                                    const TypeDescriptor* field_type) {
                     if (done) { return; }
                     if (my_key != std::string_view(name)) { return; }
                     done = true;
                     found = Locate(field_value, field_type,
                                    remaining_key, create, action);
                   });
      return found;
    }
    case TypeKind::kArray: {
      const auto* desc = AsArray(type);
      base::Tokenizer tokenizer(key, ".");
      const int index = ParseIndex(tokenizer.next(), desc->size);
      if (index < 0) { return false; }
      return Locate(ArrayElement(value, desc, index), desc->element,
                    tokenizer.remaining(), create, action);
    }
    case TypeKind::kOptional: {
      const auto* desc = AsOptional(type);
      void* const contained =
          create ? desc->get_or_emplace(value) : desc->get(value);
      if (!contained) {
        action.Apply(nullptr, nullptr);
        return true;
      }
      return Locate(contained, desc->element, key, create, action);
    }
    case TypeKind::kExternal: {
      bool found = false;
      Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
          found = Locate(inner, inner_type, key, create, action);
        });
      return found;
    }
    default: {
      action.Apply(value, type);
      return true;
    }
  }
}

class SetAction final : public LeafAction {
 public:
  explicit SetAction(const std::string_view& value) : str_(value.data()) {}

  void Apply(void* value, const TypeDescriptor* type) override {
    if (!value) { return; }

    switch (type->kind) {
      case TypeKind::kBool: {
        Store<bool>(value, std::strtoull(str_, nullptr, 0) != 0);
        return;
      }
      case TypeKind::kInt8:
      case TypeKind::kInt16:
      case TypeKind::kInt32:
      case TypeKind::kInt64: {
        StoreInteger(value, type->kind,
                     static_cast<uint64_t>(std::strtoll(str_, nullptr, 0)));
        return;
      }
      case TypeKind::kUInt8:
      case TypeKind::kUInt16:
      case TypeKind::kUInt32:
      case TypeKind::kUInt64: {
        StoreInteger(value, type->kind, std::strtoull(str_, nullptr, 0));
        return;
      }
      case TypeKind::kFloat32: {
        Store<float>(value, std::strtof(str_, nullptr));
        return;
      }
      case TypeKind::kFloat64: {
        Store<double>(value, std::strtod(str_, nullptr));
        return;
      }
      case TypeKind::kEnum: {
        const int32_t parsed =
            static_cast<int32_t>(std::strtoll(str_, nullptr, 0));
        StoreInteger(value, AsEnum(type)->underlying,
                     static_cast<uint64_t>(static_cast<int64_t>(parsed)));
        return;
      }
      default: {
        return;
      }
    }
  }

 private:
  const char* const str_;
};

class ReadAction final : public LeafAction {
 public:
  ReadAction(const base::string_span& buffer,
             AsyncWriteStream& stream,
             const ErrorCallback& callback)
      : buffer_(buffer), stream_(stream), callback_(callback) {}

  void Apply(void* value, const TypeDescriptor* type) override {
    AsyncWrite(stream_, Format(value, type), callback_);
  }

 private:
  // snprintf returns the would-have-been length, not what was
  // actually written.  Clamp to the bytes that snprintf actually
  // emitted (at most buffer_.size() - 1 because of the trailing NUL).
  std::string_view Format(void* value, const TypeDescriptor* type) {
    if (buffer_.size() == 0) { return std::string_view(); }
    const int out_size =
        FormatText(buffer_.data(), buffer_.size(), value, type);
    if (out_size < 0) { return std::string_view(); }
    const std::size_t written = std::min<std::size_t>(
        out_size, buffer_.size() - 1);
    return std::string_view(buffer_.data(), written);
  }

  const base::string_span buffer_;
  AsyncWriteStream& stream_;
  const ErrorCallback& callback_;
};

/////////////////////////////////////
// Enumerate

struct PrefixNode {
  std::string_view prefix;
  const PrefixNode* parent;
};

/// Each Run() formats as many fields as fit into the buffer, then
/// writes them.  When that write completes, the next Run() resumes
/// after the last field emitted, identified by its path: the field or
/// element index at each level of nesting.  Earlier fields are
/// skipped by comparing indices, without descending into them, and
/// arrays are entered directly at the saved element.
class Enumerator {
 public:
  explicit Enumerator(EnumerateContext* context) : context_(context) {}

  /// @return true if anything was written
  bool Run() {
    const PrefixNode root{context_->root_prefix, nullptr};
    Fields(context_->object, context_->type, &root, 0,
           context_->last_depth != 0);
    if (used_ == 0) { return false; }

    AsyncWrite(*context_->stream,
               std::string_view(context_->buffer.data(), used_),
               [ctx = context_](const error_code& error) {
                 if (error) { ctx->callback(error); return; }

                 if (!Enumerator(ctx).Run()) {
                   // We have finished with everything.
                   ctx->callback({});
                 } else {
                   // This walk should have enqueued another callback,
                   // we are done.
                 }
               });
    return true;
  }

 private:
  static constexpr int kMaxDepth = EnumerateContext::kMaxDepth;

  /// Visits the fields of one structure.  If @p following, then every
  /// index above @p depth matches the last emitted field.
  class FieldWalker final : public FieldVisitor {
   public:
    FieldWalker(Enumerator* parent, const PrefixNode* node,
                int depth, bool following)
        : parent_(parent), node_(node), depth_(depth),
          following_(following) {}

    void Field(const char* name, void* value,
               const TypeDescriptor* type) override {
      const int index = index_++;
      if (parent_->done_) { return; }
      bool follow = following_;
      if (follow) {
        const int last = parent_->context_->last_path[depth_];
        if (index < last) { return; }
        follow = (index == last);
      }
      parent_->path_[depth_] = index;
      parent_->Value(node_, name, value, type, depth_ + 1, follow);
    }

   private:
    Enumerator* const parent_;
    const PrefixNode* const node_;
    const int depth_;
    const bool following_;
    int index_ = 0;
  };

  void Fields(void* value, const TypeDescriptor* type,
              const PrefixNode* node, int depth, bool following) {
    if (type->kind == TypeKind::kExternal) {
      Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
          Fields(inner, inner_type, node, depth, following);
        });
      return;
    }
    if (depth >= kMaxDepth) {
      // Nested too deeply to record where we are.
      return;
    }
    FieldWalker walker(this, node, depth, following);
    static_cast<const StructDescriptor*>(type)->visit_fields(value, walker);
  }

  /// @param depth the number of indices in the path to @p value
  __attribute__((noinline))
  void Value(const PrefixNode* node, const char* name,
             void* value, const TypeDescriptor* type,
             int depth, bool following) {
    if (done_) { return; }

    // If the structure no longer matches the last emitted path, just
    // stop following it.
    if (following && depth >= context_->last_depth) {
      if (type->kind == TypeKind::kStruct ||
          type->kind == TypeKind::kArray) {
        following = false;
      }
    }

    switch (type->kind) {
      case TypeKind::kStruct: {
        const PrefixNode child{name, node};
        Fields(value, type, &child, depth, following);
        return;
      }
      case TypeKind::kArray: {
        if (depth >= kMaxDepth) { return; }
        const auto* desc = AsArray(type);
        const PrefixNode child{name, node};
        const int start = following ? context_->last_path[depth] : 0;
        for (int i = start; i < desc->size && !done_; i++) {
          char number[13] = {};
          ::snprintf(number, sizeof(number), "%d", i);
          path_[depth] = i;
          Value(&child, number, ArrayElement(value, desc, i), desc->element,
                depth + 1, following && i == start);
        }
        return;
      }
      case TypeKind::kOptional: {
        const auto* desc = AsOptional(type);
        void* const contained = desc->get(value);
        if (!contained) {
          Leaf(node, name, nullptr, nullptr, depth, following);
        } else {
          Value(node, name, contained, desc->element, depth, following);
        }
        return;
      }
      case TypeKind::kExternal: {
        Unwrap(value, type, [&](void* inner, const TypeDescriptor* inner_type) {
            Value(node, name, inner, inner_type, depth, following);
          });
        return;
      }
      default: {
        Leaf(node, name, value, type, depth, following);
        return;
      }
    }
  }

  void Leaf(const PrefixNode* node, const char* name,
            void* value, const TypeDescriptor* type,
            int depth, bool following) {
    // This is the field emitted last time.
    if (following && depth == context_->last_depth) { return; }
    Emit(node, name, value, type, depth);
  }

  // This, and FormatField, are kept out of line so that their buffers
  // are not part of every level of recursion.
  __attribute__((noinline))
  void Emit(const PrefixNode* node, const char* name,
            void* value, const TypeDescriptor* type, int depth) {
    // When appending to a batch, the line must fit entirely.
    const bool strict = used_ != 0;
    const int size = FormatField(
        context_->buffer.begin() + used_, context_->buffer.end(),
        strict, node, name, value, type);
    if (size == 0 && used_ != 0) {
      // This did not fit after the fields already formatted, so it
      // starts the next batch.
      done_ = true;
      return;
    }

    // Either this fit, or it will not fit even in an empty buffer, in
    // which case it is skipped.
    used_ += size;
    for (int i = 0; i < depth; i++) {
      context_->last_path[i] = path_[i];
    }
    context_->last_depth = depth;
  }

  /// If not @p strict, then as the original implementation did, a
  /// parent prefix which does not fit is silently omitted.
  static int FormatPrefix(char** current, char* end, const PrefixNode* node,
                          bool strict) {
    if (node->parent) {
      if (FormatPrefix(current, end, node->parent, strict) && strict) {
        return 1;
      }
    }

    // Would we overflow?
    if (static_cast<std::ptrdiff_t>(node->prefix.size() + 2) >
        (end - *current)) {
      return 1;
    }

    for (const char c : node->prefix) {
      **current = c;
      ++(*current);
    }

    **current = '.';
    ++(*current);
    return 0;
  }

  __attribute__((noinline))
  /// Format one line into [begin, end).
  ///
  /// @return the size, or 0 if it did not fit
  static int FormatField(char* const begin, char* const end,
                         bool strict,
                         const PrefixNode* node,
                         const char* name_str,
                         void* value,
                         const TypeDescriptor* type) {
    char* it = begin;

    if (FormatPrefix(&it, end, node, strict)) {
      return 0;
    }

    const std::string_view name(name_str);
    if (static_cast<std::ptrdiff_t>(name.size() + 3) > (end - it)) {
      return 0;
    }

    for (const char c : name) {
      *it = c;
      ++it;
    }

    *it = ' ';
    ++it;

    // Reserve room for the trailing "\r\n" so the value cannot
    // consume it, then bail out if the formatted value does not fit.
    const auto value_avail = (end - it) - 2;
    if (value_avail < 0) { return 0; }

    // snprintf reserves the last byte for a NUL we do not want to
    // emit, so format into a stack scratch and copy.  40 bytes is
    // ample for any scalar (int64 min: 20 chars, "%g" double: ~24).
    char scratch[40];
    const int result = FormatText(scratch, sizeof(scratch), value, type);
    if (result < 0 ||
        result >= static_cast<int>(sizeof(scratch)) ||
        static_cast<std::ptrdiff_t>(result) > value_avail) {
      return 0;
    }
    for (int i = 0; i < result; i++) {
      *it = scratch[i];
      ++it;
    }

    *it = '\r';
    ++it;
    *it = '\n';
    ++it;
    return it - begin;
  }

  EnumerateContext* const context_;
  uint16_t path_[kMaxDepth] = {};
  int used_ = 0;
  bool done_ = false;
};

}  // namespace

int TypeErasedSerializableHandler::WriteBinary(base::WriteStream& stream) {
  telemetry::WriteStream tstream(stream);
  WriteValue(tstream, item_, root_->type);
  return 0;
}

void TypeErasedSerializableHandler::WriteSchema(base::WriteStream& stream) {
  struct Context {
    base::WriteStream* stream;
    const TypeDescriptor* type;
  };
  Context context{&stream, root_->type};
  root_->with_default([](void* context_ptr, void* object) {
      auto* ctx = static_cast<Context*>(context_ptr);
      telemetry::WriteStream tstream(*ctx->stream);
      WriteSchemaType(tstream, object, ctx->type);
    }, &context);
}

int TypeErasedSerializableHandler::ReadBinary(base::ReadStream& stream) {
  telemetry::ReadStream tstream(stream);
  BinaryReader reader(tstream);
  reader.Field(nullptr, item_, root_->type);
  return reader.error() ? 1 : 0;
}

int TypeErasedSerializableHandler::Set(const std::string_view& key,
                                       const std::string_view& value) {
  SetAction action(value);
  return Locate(item_, root_->type, key, true, action) ? 0 : 1;
}

void TypeErasedSerializableHandler::Enumerate(
    EnumerateContext* context,
    const base::string_span& buffer,
    const std::string_view& prefix,
    AsyncWriteStream& stream,
    ErrorCallback callback) {
  context->root_prefix = prefix;
  context->stream = &stream;
  context->buffer = buffer;
  context->callback = callback;
  context->last_depth = 0;
  context->object = item_;
  context->type = root_->type;

  if (!Enumerator(context).Run()) {
    // Nothing was emitted, so no AsyncWrite chain will run to signal
    // completion.  Fire the callback here to avoid hanging any caller
    // driving a state machine off it.
    callback({});
  }
}

int TypeErasedSerializableHandler::Read(const std::string_view& key,
                                        const base::string_span& buffer,
                                        AsyncWriteStream& stream,
                                        ErrorCallback callback) {
  ReadAction action(buffer, stream, callback);
  return Locate(item_, root_->type, key, false, action) ? 0 : 1;
}

void TypeErasedSerializableHandler::SetDefault() {
  root_->set_default(item_);
}

}
}
