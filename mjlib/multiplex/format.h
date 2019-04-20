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

/// @file
///
/// The multiplex protocol is intended to be used over half or
/// full-duplex serial connections.  It presents a logical packet
/// based interface, assumed to be a client/server bus with a single
/// client and one or more servers.  It implements a number of
/// services.
///
/// # Common Definitions #
///  * endian-ness
///    - all primitive types are in least signficant byte first
///  * varuint
///    - A sequence of one or more uint8_t values, in least
///      significant first order.  For each value, the 7 LSBs contain
///      data and if the MSB is set, it means there are more bytes
///      remaining.  At most, it may represent a single uint32_t, and
///      thus 5 bytes is the maximum valid length.
///  * float
///    - an IEEE 754 32-bit floating number in least significant byte
///      first order
///  * ID
///    - each node is identified by a 7 bit identifier, 0x7f is the
///      "broadcast" address
///
///
/// # Frame format #
///
///  * Header
///     - uint16_t => 0xab54
///     - uint8_t => source id
///        > if the high bit is set, that means a response is requested
///     - uint8_t => destination id
///     - varuint => size of payload
///     - bytes => Payload
///     - uint16_t => crc16 of entire frame including header assuming
///                   checksum field is 0x0000
///
///  * Payload
///   - subframe 1
///   - subframe 2
///
///  * Subframe
///   - varuint => subframe type
///   - [bytes] => possible subframe specific data
///
/// # Service: Register based RPC #
///
/// This service models a device which consists of up to 2**32
/// "registers".  Each register denotes a value of some kind, which
/// may be mapped into one or more different representation formats.
/// The client may set or query the value of any register.  Which
/// representation formats are valid for a given register is device
/// dependent.  The set of possible representation formats is:
///   (int8_t, int16_t, int32_t, float)
///
/// ## Subframes ##
///   0x10, 0x11, 0x12, 0x13 - write single (int8_t|int16_t|int32_t|float)
///     - varuint => register #
///     - (int8_t|int16_t|int32_t|float) => value
///   0x14, 0x15, 0x16, 0x17 - write multiple (int8_t|int16_t|int32_t|float)
///     - varuint => start register #
///     - varuint => number of registers
///     - N x (int8_t|int16_t|int32_t|float) => values
///
///   0x18, 0x19, 0x1a, 0x1b - read single (int8_t|int16_t|int32_t|float)
///     - varuint => register #
///   0x1c, 0x1d, 0x1e, 0x1f - read multiple (int8_t|int16_t|int32_t|float)
///     - varuint => start register #
///     - varuint => number of registers
///
///   0x20, 0x21, 0x22, 0x23 - reply single (int8_t|int16_t|int32_t|float)
///     - varuint => register #
///     - (int8_t|int16_t|int32_t|float) => value
///   0x24, 0x25, 0x26, 0x27 - reply multiple (int8_t|int16_t|int32_t|float)
///     - varuint => start register #
///     - varuint => number of registers
///     - N x (int8_t|int16_t|int32_t|float) => values
///   0x28 - write error
///     - varuint => register #
///     - varuint => error #
///   0x29 - read error
///     - varuint => register #
///     - varuint => error #
///
/// Any frame that contains a "read" command will have a response
/// frame where each requested register is named exactly once.  It is
/// not required that the responses use the exact same single/multiple
/// formulation as long as each is mentioned once.
///
/// # Service: Tunneled Stream #
///
/// The tunneled stream service models a simple byte stream, where the
/// client must poll the servers for data.
///
/// ## Subframes ##
///
///  0x40 - client data on channel
///    - varuint => channel
///    - varuint => number of bytes sent from client
///    - N x uint8_t bytes
///  0x41 - server data on channel
///    - varuint => channel
///    - varuint => number of bytes sent from server
///    - N x uint8_t bytes
///
/// In response to receiving a frame with the 0x40 subframe, the slave
/// should respond with a 0x41 subframe whether or not it currently
/// has data.
///
/// A frame that contains a tunneled stream subframe may contain
/// exactly 1 subframe total.

#include <cstdint>
#include <variant>

namespace mjlib {
namespace multiplex {

struct Format {
  static constexpr uint16_t kHeader = 0xab54;
  static constexpr int kHeaderSize = 4;
  static constexpr int kMaxVaruintSize = 5;
  static constexpr int kMinVaruintSize = 1;
  static constexpr int kCrcSize = 2;
  static constexpr uint8_t kBroadcastId = 0x7f;

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

inline int GetVaruintSize(uint32_t value) {
  int result = 0;
  do {
    value >>= 7;
    result++;
  } while (value);
  return result;
}

}
}
