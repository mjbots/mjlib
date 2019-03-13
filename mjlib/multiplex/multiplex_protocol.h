// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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
#include <variant>

#include "mjlib/base/visitor.h"

namespace mjlib {
namespace micro {

struct MultiplexProtocol {
  static constexpr uint16_t kHeader = 0xab54;
  static constexpr int kHeaderSize = 4;
  static constexpr int kMaxVaruintSize = 5;
  static constexpr int kMinVaruintSize = 1;
  static constexpr int kCrcSize = 2;

  enum class Subframe : uint8_t {
    // # Register RPC #
    kWriteSingleBase = 0x10,
    kWriteSingleInt8 = 0x10,
    kWriteSingleInt16 = 0x11,
    kWriteSingleInt32 = 0x12,
    kWriteSingleFloat = 0x13,

    kWriteMultipleBase = 0x14,
    kWriteMultipleInt8 = 0x14,
    kWriteMultipleInt16 = 0x15,
    kWriteMultipleInt32 = 0x16,
    kWriteMultipleFloat = 0x17,

    kReadSingleBase = 0x18,
    kReadSingleInt8 = 0x18,
    kReadSingleInt16 = 0x19,
    kReadSingleInt32 = 0x1a,
    kReadSingleFloat = 0x1b,

    kReadMultipleBase = 0x1c,
    kReadMultipleInt8 = 0x1c,
    kReadMultipleInt16 = 0x1d,
    kReadMultipleInt32 = 0x1e,
    kReadMultipleFloat = 0x1f,

    kReplySingleBase = 0x20,
    kReplyMultipleBase = 0x24,
    kWriteError = 0x28,
    kReadError = 0x29,

    // # Tunneled Stream #
    kClientToServer = 0x40,
    kServerToClient = 0x41,
  };

  using Register = uint32_t;

  using Value = std::variant<int8_t, int16_t, int32_t, float>;
  // Either a Value, or an error code.
  using ReadResult = std::variant<Value, uint32_t>;
};


class MultiplexProtocolClient : public MultiplexProtocol {
 public:

 private:
  class Impl;
  PoolPtr<Impl> impl_;
};

}
}
