// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>

#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

/// Take a buffer sequence, and return one whose start is offset by @p
/// offset (and thus its overall size is reduced).
template <typename Buffers>
BufferSequence<typename Buffers::value_type> OffsetBufferSequence(
    Buffers buffers, size_t offset) {
  BOOST_ASSERT(offset < boost::asio::buffer_size(buffers));

  using Buffer = typename Buffers::value_type;
  std::vector<Buffer> result;

  size_t consumed_so_far = 0;
  for (auto& buffer : buffers) {
    if (consumed_so_far > offset) {
      // Don't bother counting anymore.
      result.push_back(buffer);
    } else if (consumed_so_far + buffer.size() < offset) {
      // We are skipping this one entirely.
      consumed_so_far += buffer.size();
    } else {
      // We must need to use part of this buffer.
      const size_t to_skip = offset - consumed_so_far;
      result.push_back(buffer + to_skip);
      consumed_so_far += buffer.size();
    }
  }

  return result;
}

}
}
