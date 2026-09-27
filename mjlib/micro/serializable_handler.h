// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <cstdint>
#include <string_view>

#include "mjlib/base/stream.h"
#include "mjlib/base/string_span.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/async_types.h"
#include "mjlib/micro/type_descriptor.h"

namespace mjlib {
namespace micro {

/// State for an in-progress SerializableHandlerBase::Enumerate.  It
/// must remain valid until the callback is invoked.
struct EnumerateContext {
  /// The deepest nesting of structures and arrays that can be
  /// enumerated.
  static constexpr int kMaxDepth = 12;

  std::string_view root_prefix;
  base::string_span buffer;
  AsyncWriteStream* stream = nullptr;
  ErrorCallback callback;

  void* object = nullptr;
  const TypeDescriptor* type = nullptr;

  /// The path to the most recently emitted field: the field or element
  /// index at each level of nesting.
  uint8_t last_depth = 0;
  uint16_t last_path[kMaxDepth] = {};
};

namespace detail {
struct EnumerateArchive {
  using Context = EnumerateContext;
};
}

class SerializableHandlerBase {
 public:
  virtual ~SerializableHandlerBase() {}

  virtual int WriteBinary(base::WriteStream&) = 0;
  virtual void WriteSchema(base::WriteStream&) = 0;
  virtual int ReadBinary(base::ReadStream&) = 0;
  virtual int Set(const std::string_view& key,
                  const std::string_view& value) = 0;
  virtual void Enumerate(EnumerateContext*,
                         const base::string_span& buffer,
                         const std::string_view& prefix,
                         AsyncWriteStream&,
                         ErrorCallback) = 0;
  virtual int Read(const std::string_view& key,
                   const base::string_span& buffer,
                   AsyncWriteStream&,
                   ErrorCallback) = 0;
  virtual void SetDefault() = 0;
};

/// Implements SerializableHandlerBase for any type, given a
/// TypeDescriptor.  All serializable types share this single
/// implementation, so that the flash cost of each new type is only
/// its constant descriptors.
class TypeErasedSerializableHandler : public SerializableHandlerBase {
 public:
  /// The per-type information needed beyond the TypeDescriptor.
  struct Root {
    const TypeDescriptor* type;
    void (*set_default)(void* object);
    WithDefaultFunction with_default;
  };

  TypeErasedSerializableHandler(void* item, const Root* root)
      : item_(item), root_(root) {}
  ~TypeErasedSerializableHandler() override {}

  int WriteBinary(base::WriteStream&) override;
  void WriteSchema(base::WriteStream&) override;
  int ReadBinary(base::ReadStream&) override;
  int Set(const std::string_view& key,
          const std::string_view& value) override;
  void Enumerate(EnumerateContext*,
                 const base::string_span& buffer,
                 const std::string_view& prefix,
                 AsyncWriteStream&,
                 ErrorCallback) override;

  /// Write a value of a sub-item to an asynchronous stream.
  ///
  /// @param key - A dot separated identifier for the sub-item to
  ///     read.
  /// @param buffer - A working buffer to use.  It must remain valid
  ///     until the callback is invoked and must be larger than the
  ///     largest value to be returned.
  ///
  /// @return non-zero if the item was not found
  int Read(const std::string_view& key,
           const base::string_span& buffer,
           AsyncWriteStream&,
           ErrorCallback) override;
  void SetDefault() override;

 private:
  void* const item_;
  const Root* const root_;
};

/// The constant information needed to handle a T.
template <typename T>
inline constexpr TypeErasedSerializableHandler::Root kSerializableRoot = {
  GetTypeDescriptor<T>(),
  &detail::AssignValue<T>,
  &detail::WithDefaultInitialized<T>,
};

/// A convenience wrapper for TypeErasedSerializableHandler.  Note,
/// that each instantiation has its own vtable, so code concerned
/// about size should use TypeErasedSerializableHandler with
/// kSerializableRoot<T> directly.
template <typename T>
class SerializableHandler : public TypeErasedSerializableHandler {
 public:
  static_assert(base::IsSerializable<T>(),
                "SerializableHandler requires a serializable structure");

  SerializableHandler(T* item)
      : TypeErasedSerializableHandler(item, &kSerializableRoot<T>) {}
};

}
}
