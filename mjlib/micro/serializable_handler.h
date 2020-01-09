// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include <string_view>

#include "mjlib/base/stream.h"
#include "mjlib/base/string_span.h"

#include "mjlib/telemetry/binary_read_archive.h"
#include "mjlib/telemetry/binary_write_archive.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/async_types.h"
#include "mjlib/micro/serializable_handler_detail.h"

namespace mjlib {
namespace micro {

class SerializableHandlerBase {
 public:
  virtual ~SerializableHandlerBase() {}

  virtual int WriteBinary(base::WriteStream&) = 0;
  virtual void WriteSchema(base::WriteStream&) = 0;
  virtual int ReadBinary(base::ReadStream&) = 0;
  virtual int Set(const std::string_view& key,
                  const std::string_view& value) = 0;
  virtual void Enumerate(detail::EnumerateArchive::Context*,
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

template <typename T>
class SerializableHandler : public SerializableHandlerBase {
 public:
  SerializableHandler(T* item) : item_(item) {}
  ~SerializableHandler() override {}

  int WriteBinary(base::WriteStream& stream) override final {
    telemetry::BinaryWriteArchive archive(stream);
    archive.Accept(item_);
    return 0;
  }

  void WriteSchema(base::WriteStream& stream) override final {
    telemetry::BinarySchemaArchive archive(stream);
    T temporary;
    archive.Accept(&temporary);
  }

  int ReadBinary(base::ReadStream& stream) override final {
    telemetry::BinaryReadArchive archive(stream);
    archive.Accept(item_);
    if (archive.error()) { return 1; }
    return 0;
  }

  int Set(const std::string_view& key,
          const std::string_view& value) override final {
    auto archive = detail::SetArchive(key, value);
    archive.Accept(item_);
    return archive.found() ? 0 : 1;
  }

  void Enumerate(detail::EnumerateArchive::Context* context,
                 const base::string_span& buffer,
                 const std::string_view& prefix,
                 AsyncWriteStream& stream,
                 ErrorCallback callback) override final {
    context->root_prefix = prefix;
    context->stream = &stream;
    context->buffer = buffer;
    context->callback = callback;
    context->current_field_index_to_write = 0;
    context->evaluate_enumerate_archive = [this, context]() {
      uint16_t current_index = 0;
      bool done = false;
      detail::EnumerateArchive(
          context, context->root_prefix,
          &current_index, &done, nullptr).Accept(this->item_);
      return done;
    };

    context->evaluate_enumerate_archive();
  }

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
           AsyncWriteStream& stream,
           ErrorCallback callback) override final {
    detail::ReadArchive archive(key, buffer, stream, callback);
    archive.Accept(item_);
    return archive.found() ? 0 : 1;
  }

  void SetDefault() override final {
    *item_ = T();
  }

 private:
  T* const item_;
};

}
}
