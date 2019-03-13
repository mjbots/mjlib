// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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
#include <limits>

#include "mjlib/base/assert.h"
#include "mjlib/base/string_span.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/error.h"

namespace mjlib {
namespace micro {

struct AsyncReadUntilContext {
  AsyncReadStream* stream = nullptr;
  base::string_span buffer;
  SizeCallback callback;
  const char* delimiters = nullptr;
};

namespace detail {
inline void AsyncReadUntilHelper(AsyncReadUntilContext& context,
                                 uint16_t position) {
  auto handler =
      [ctx=&context,
       position] (error_code error, std::size_t size) {
    if (error) {
      ctx->callback(error, position + size);
      return;
    }

    MJ_ASSERT(size == 0 || size == 1);

    if (std::strchr(ctx->delimiters,
                    ctx->buffer.data()[position]) != nullptr) {
      ctx->callback({}, position + size);
      return;
    }

    if (position + 1 == static_cast<int>(ctx->buffer.size())) {
      // We overfilled our buffer without getting a terminator.
      ctx->callback(errc::kDelimiterNotFound, position);
      return;
    }

    AsyncReadUntilHelper(*ctx, position + 1);
  };

  context.stream->AsyncReadSome(
      base::string_span(context.buffer.data() + position,
                        context.buffer.data() + position + 1), handler);
}
}

inline void AsyncReadUntil(AsyncReadUntilContext& context) {
  MJ_ASSERT(context.buffer.size() < std::numeric_limits<uint16_t>::max());
  detail::AsyncReadUntilHelper(context, 0);
}

inline void AsyncIgnoreUntil(AsyncReadUntilContext& context) {
  context.stream->AsyncReadSome(
      base::string_span(context.buffer.data(), context.buffer.data() + 1),
      [ctx=&context](error_code error, std::size_t) {
        if (error) {
          ctx->callback(error, 0);
          return;
        }

        if (std::strchr(ctx->delimiters,
                        ctx->buffer.data()[0]) != nullptr) {
          ctx->callback({}, 0);
          return;
        }

        AsyncIgnoreUntil(*ctx);
      });
}

}
}
