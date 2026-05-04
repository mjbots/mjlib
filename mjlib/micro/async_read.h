// Copyright 2025 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

#include "mjlib/base/assert.h"
#include "mjlib/base/string_span.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/error.h"

namespace mjlib {
namespace micro {

struct AsyncStreamBuf {
  // The available capacity in the streambuf.
  base::string_span buffer;

  // Data is filled up to this point.
  size_t size = 0;

  AsyncStreamBuf() {}
  AsyncStreamBuf(base::string_span buffer_in, size_t size_in)
      : buffer(buffer_in),
        size(size_in) {}
};

struct AsyncReadUntilContext {
  AsyncReadStream* stream = nullptr;
  AsyncStreamBuf* streambuf = nullptr;
  base::string_span result;

  SizeCallback callback;
  const char* delimiters = nullptr;
};

namespace detail {
inline bool AsyncReadUntilCheck(AsyncReadUntilContext* ctx) {
  auto* const sbd = ctx->streambuf->buffer.data();

  for (uint16_t i = 0; i < ctx->streambuf->size; i++) {
    if (std::strchr(ctx->delimiters, sbd[i]) != nullptr) {
      // Copy to the result storage. Cap at the result span size:
      // streambuf and result may be sized independently by the
      // caller, and a line longer than result would otherwise
      // overflow.
      const size_t copy_size = std::min(
          static_cast<size_t>(i + 1),
          static_cast<size_t>(ctx->result.size()));
      std::memcpy(ctx->result.data(), sbd, copy_size);
      // Remove data from our streambuf, including the delimiter,
      // even if part of the line was dropped above.
      const auto new_streambuf_size =
          ctx->streambuf->size - i - 1;
      std::memmove(sbd, sbd + i + 1, new_streambuf_size);
      ctx->streambuf->size = new_streambuf_size;

      ctx->callback({}, copy_size);
      return true;
    }
  }

  if (ctx->streambuf->size ==
      static_cast<size_t>(ctx->streambuf->buffer.size())) {
    const auto reply_size = ctx->streambuf->size;

    // Empty the streambuf.
    ctx->streambuf->size = 0;

    // We overfilled our buffer without getting a terminator.
    ctx->callback(errc::kDelimiterNotFound, reply_size);
    return true;
  }

  return false;
}

inline void AsyncReadUntilHelper(AsyncReadUntilContext& context) {
  if (AsyncReadUntilCheck(&context)) {
    return;
  }

  auto handler =
      [ctx=&context] (error_code error, std::size_t size) {
        ctx->streambuf->size += size;

        if (error) {
          ctx->callback(error, ctx->streambuf->size);
          return;
        }

        if (AsyncReadUntilCheck(ctx)) {
          return;
        }

        AsyncReadUntilHelper(*ctx);
      };

  // Attempt to read as much data as we have room for in our streambuf.
  context.stream->AsyncReadSome(
      base::string_span(
          context.streambuf->buffer.data() +
          context.streambuf->size,
          context.streambuf->buffer.data() +
          context.streambuf->buffer.size()), handler);
}
}

inline void AsyncReadUntil(AsyncReadUntilContext& context) {
  detail::AsyncReadUntilHelper(context);
}

namespace detail {
inline bool AsyncIgnoreUntilCheck(AsyncReadUntilContext* ctx) {
  // Is there a delimeter already in our streambuf?  If so, delete
  // everything until then and return true.  If there isn't, then
  // delete everything entirely in the streambuf and return false.
  auto* const sbd = ctx->streambuf->buffer.data();

  for (uint16_t i = 0; i < ctx->streambuf->size; i++) {
    if (std::strchr(ctx->delimiters, sbd[i]) != nullptr) {
      const auto new_streambuf_size =
          ctx->streambuf->size - i - 1;
      std::memmove(sbd, sbd + i + 1, new_streambuf_size);
      ctx->streambuf->size = new_streambuf_size;
      return true;
    }
  }

  // No delimeters found.  Delete it all.
  ctx->streambuf->size = 0;
  return false;
}
}

inline void AsyncIgnoreUntil(AsyncReadUntilContext& context) {
  const auto done = detail::AsyncIgnoreUntilCheck(&context);
  if (done) {
    context.callback({}, 0);
    return;
  }

  context.stream->AsyncReadSome(
      base::string_span(context.streambuf->buffer.data(),
                        context.streambuf->buffer.size()),
      [ctx=&context](error_code error, std::size_t size) {
        if (error) {
          ctx->callback(error, 0);
          return;
        }

        ctx->streambuf->size += size;

        AsyncIgnoreUntil(*ctx);
      });
}

}
}
