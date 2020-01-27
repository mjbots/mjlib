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
///   0x00, 0x04, 0x08, 0x0c - write (int8_t|int16_t|int32_t|float)
///     - varuint => number of registers (may be optionally encoded as
///       a non-zero 2 LSBs)
///     - varuint => start register #
///     - N x (int8_t|int16_t|int32_t|float) => values
///   0x10, 0x14, 0x18, 0x1c - read (int8_t|int16_t|int32_t|float)
///     - varuint => number of registers (may be optionally encoded as
///       a non-zero 2 LSBs)
///     - varuint => start register #
///   0x20, 0x24, 0x28, 0x2c - reply (int8_t|int16_t|int32_t|float)
///     - varuint => number of registers (may be optionally encoded as
///       a non-zero 2 LSBs)
///     - varuint => start register #
///     - N x (int8_t|int16_t|int32_t|float) => values
///   0x30 - write error
///     - varuint => register #
///     - varuint => error #
///   0x31 - read error
///     - varuint => register #
///     - varuint => error #
///   0x50 - nop
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
///  0x42 - client poll server
///    - varuint => channel
///    - varuint => maximum number of bytes to reply
///
/// In response to receiving a frame with the 0x40 subframe, the slave
/// should respond with a 0x41 subframe whether or not it currently
/// has data.
///
/// In response to receiving a 0x42 subframe, the slave should respond
/// with an 0x41 subframe whether or not it has data, with a maximum
/// size as specified by the client.
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
    kWriteBase = 0x00,
    kWriteInt8 = 0x00,
    kWriteInt16 = 0x04,
    kWriteInt32 = 0x08,
    kWriteFloat = 0x0c,

    kReadBase = 0x10,
    kReadInt8 = 0x10,
    kReadInt16 = 0x14,
    kReadInt32 = 0x18,
    kReadFloat = 0x1c,

    kReplyBase = 0x20,
    kReplyInt8 = 0x20,
    kReplyInt16 = 0x24,
    kReplyInt32 = 0x28,
    kReplyFloat = 0x2c,

    kWriteError = 0x30,
    kReadError = 0x31,

    // # Tunneled Stream #
    kClientToServer = 0x40,
    kServerToClient = 0x41,
    kClientPollServer = 0x42,

    kNop = 0x50,
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
