// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/event.h"

namespace mjlib {
namespace micro {

/// Provides a programmatic mechanism for creating a "pipe", where two
/// streams write and read from each other.
class StreamPipe {
 public:
  /// @param event_poster - all callbacks are invoked using this event
  /// queue
  StreamPipe(EventPoster event_poster) noexcept;

  AsyncStream* side_a() noexcept;
  AsyncStream* side_b() noexcept;

 private:
  const EventPoster poster_;

  class Side : public AsyncStream {
   public:
    Side(StreamPipe* parent, Side* other) : parent_(parent), other_(other) {}

    void AsyncReadSome(const base::string_span& buffer,
                       const SizeCallback& callback) override;

    void AsyncWriteSome(const std::string_view& buffer,
                        const SizeCallback& callback) override;

   private:
    StreamPipe* const parent_;
    Side* const other_;

    base::string_span outstanding_read_buffer_;
    SizeCallback outstanding_read_callback_;
    SizeCallback pending_read_callback_;

    std::string_view outstanding_write_buffer_;
    SizeCallback outstanding_write_callback_;
    SizeCallback pending_write_callback_;
  };

  Side side_a_;
  Side side_b_;
};

}
}
