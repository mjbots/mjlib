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

#include <boost/asio/error.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/time_conversions.h"

#ifdef __cpp_exceptions
#include "mjlib/base/system_error.h"
#endif

#include "mjlib/telemetry/types.h"

/// @file
///
/// See README.md for documentation of this format.
///
/// This defines C++ constants for the binary serialization and helper
/// utilities for reading and writing primitive types.

namespace mjlib {
namespace telemetry {

struct Format {
  static constexpr const char* kHeader = "TLOG0003";
  static constexpr int kMaxStringSize = 1 << 24;

  enum class Type {
    kFinal = 0,
    kNull,
    kBoolean,
    kFixedInt,
    kFixedUInt,
    kVarint,
    kVaruint,
    kFloat32,
    kFloat64,
    kBytes,
    kString,

    kObject = 64,
    kEnum,
    kArray,
    kMap,
    kUnion,
    kTimestamp,
    kDuration,
  };

  enum class FieldFlags {
  };

  enum class ObjectFlags {
  };

  enum class HeaderFlags {
  };

  enum class BlockType {
    kSchema = 1,
    kData = 2,
    kIndex = 3,
    kCompressionDictionary = 4,
    kSeekMarker = 5,
  };

  enum class BlockSchemaFlags {
  };

  enum class BlockDataFlags {
    // The following flags define optional fields which will be
    // present after the flags field and before the data itself.  If
    // multiple flags are present, the optional data is present in the
    // order the flags are defined here.

    /// The number of bytes prior to the start of this block where the
    /// previous data block of the same identifier can be found.  0 if
    /// no such block exists.
    ///
    ///   * varuint
    kPreviousOffset = 1 << 0,

    /// The CRC32 of the binary schema associated with this data
    /// record.  * uint32_t
    kSchemaCRC = 1 << 1,

    /// The DataObject is compressed with the "ZStandard" compression
    /// algorithm.  If the optional *varuint* is non-zero, it uses the
    /// compression dictionary with that number.
    ///
    ///  * varuint
    kZStandard = 1 << 2,

    /// This object was written at a specific time.
    ///
    ///  * fixedint64
    kTimestamp = 1 << 3,


    // The following flags do not require that additional data be stored.

    /// The DataObject is compressed with the "snappy" compression algorithm.
    kSnappy = 1 << 8,
  };
};

/// This provides C++ APIs for writing primitive types.
class WriteStream {
 public:
  WriteStream(base::WriteStream& base) : base_(base) {}

  base::WriteStream& base() { return base_; }

  void WriteString(const std::string_view& data) {
    WriteVaruint(data.size());
    RawWrite(data);
  }

  void Write(bool value) {
    WriteScalar<uint8_t>(value ? 1 : 0);
  }

  void Write(int8_t value) { WriteScalar(value); }
  void Write(uint8_t value) { WriteScalar(value); }
  void Write(int16_t value) { WriteScalar(value); }
  void Write(uint16_t value) { WriteScalar(value); }
  void Write(int32_t value) { WriteScalar(value); }
  void Write(uint32_t value) { WriteScalar(value); }
  void Write(int64_t value) { WriteScalar(value); }
  void Write(uint64_t value) { WriteScalar(value); }
  void Write(float value) { WriteScalar(value); }
  void Write(double value) { WriteScalar(value); }

  // We encode many enumeration types as varuints.  This makes writing
  // them much more convenient.
  template <typename T>
  void WriteVaruint(T value) {
    WriteVaruintReal(static_cast<uint64_t>(value));
  }

  void WriteVaruintReal(uint64_t value) {
    do {
      uint8_t word = value & 0x7f;
      bool more = value > 0x7f;
      if (more) { word |= 0x80; }
      Write(word);
      value >>= 7;
    } while (value);
  }

  void WriteVarint(int64_t value) {
    WriteVaruint(static_cast<uint64_t>((value >> 63) ^ (value << 1)));
  }

  void Write(boost::posix_time::ptime timestamp) {
    WriteScalar(base::ConvertPtimeToEpochMicroseconds(timestamp));
  }

  void RawWrite(const std::string_view& data) {
    base_.write(data);
  }

 private:
  template <typename T>
  void WriteScalar(T value) {
#ifndef __ORDER_LITTLE_ENDIAN__
#error "Only little endian architectures supported"
#endif
    // TODO jpieper: Support big-endian.
    RawWrite({reinterpret_cast<const char*>(&value), sizeof(value)});
  }

  base::WriteStream& base_;
};

/// This provides C++ APIs for reading primitive types.
class ReadStream {
 public:
  ReadStream(base::ReadStream& base) : base_(base) {}

  base::ReadStream& base() { return base_; }

  void Ignore(std::streamsize size) {
    base_.ignore(size);
    if (base_.gcount() != size) {
#ifdef __cpp_exceptions
      throw base::system_error(boost::system::error_code(boost::asio::error::eof));
#else
      MJ_ASSERT(false);
#endif
    }
  }

  template <typename T>
  T Read() {
    return ReadScalar<T>();
  }

  std::string ReadString() {
    auto size = ReadVaruint();
    if (size > Format::kMaxStringSize) {
      MJ_ASSERT(false);
    }
    std::string result(size, static_cast<char>(0));
    RawRead(&result[0], size);
    return result;
  }

  boost::posix_time::ptime ReadTimestamp() {
    return base::ConvertEpochMicrosecondsToPtime(Read<int64_t>());
  }

  uint64_t ReadVaruint() {
    uint64_t result = 0;
    int position = 0;
    uint8_t value = 0;
    do {
      value = Read<uint8_t>();
      result |= static_cast<uint64_t>(value & 0x7f) << position;
      position += 7;
    } while (value >= 0x80);

    return result;
  }

  int64_t ReadVarint() {
    auto encoded = ReadVaruint();
    return (encoded >> 1) - (encoded & 1) * encoded;
  }

 private:
  template <typename T>
  T ReadScalar() {
    T result = T{};
    RawRead(reinterpret_cast<char*>(&result), sizeof(result));
    return result;
  }

  void RawRead(char* out, std::streamsize size) {
    base_.read({out, size});
    if (base_.gcount() != size) {
#ifdef __cpp_exceptions
      throw base::system_error(boost::system::error_code(boost::asio::error::eof));
#else
      MJ_ASSERT(false);
#endif
    }
  }

  base::ReadStream& base_;
};

}
}
