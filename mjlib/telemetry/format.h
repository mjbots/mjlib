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

#include <optional>

#include "mjlib/base/assert.h"
#include "mjlib/base/bytes.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/time_conversions.h"

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

    kObject = 16,
    kEnum,
    kArray,
    kFixedArray,
    kMap,
    kUnion,
    kTimestamp,
    kDuration,

    kLastType = kDuration,
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
    kNumTypes = kSeekMarker,
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

    /// This object was written at a specific time.
    ///
    ///  * fixedint64
    kTimestamp = 1 << 1,

    /// The CRC32 of the entire block, including the type and size,
    /// assuming the CRC field is all 0.
    ///
    ///  * fixeduint32
    kChecksum = 1 << 2,


    // The following flags do not require that additional data be stored.

    /// The DataObject is compressed with the "snappy" compression
    /// algorithm.
    kSnappy = 1 << 4,
  };

  static uint64_t GetVaruintSize(uint64_t value) {
    uint64_t result = 0;
    do {
      result++;
      if (value <= 0x7f) { break; }
      value >>= 7;
    } while (value);
    return result;
  }
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
      const bool more = value > 0x7f;
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
  }

  template <typename T>
  std::optional<T> Read() {
    return ReadScalar<T>();
  }

  std::optional<std::string> ReadString() {
    const auto maybe_size = ReadVaruint();
    if (!maybe_size) {
      return {};
    }
    const auto size = *maybe_size;
    if (size > Format::kMaxStringSize) {
      MJ_ASSERT(false);
    }
    std::string result(size, static_cast<char>(0));
    if (!RawRead(&result[0], size)) {
      return {};
    }
    return result;
  }

  std::optional<boost::posix_time::ptime> ReadTimestamp() {
    const auto maybe_data = Read<int64_t>();
    if (!maybe_data) { return {}; }
    return base::ConvertEpochMicrosecondsToPtime(*maybe_data);
  }

  std::optional<uint64_t> ReadVaruint() {
    uint64_t result = 0;
    int position = 0;
    uint8_t value = 0;
    do {
      const auto maybe_value = Read<uint8_t>();
      if (!maybe_value) { return {}; }
      value = *maybe_value;

      result |= static_cast<uint64_t>(value & 0x7f) << position;
      position += 7;
      // TODO jpieper: Handle malformed values that overflow a uint64.
    } while (value >= 0x80);

    return result;
  }

  std::optional<int64_t> ReadVarint() {
    const auto maybe_encoded = ReadVaruint();
    if (!maybe_encoded) { return {}; }
    const auto encoded = *maybe_encoded;
    return (encoded >> 1) - (encoded & 1) * encoded;
  }

  bool RawRead(char* out, std::streamsize size) {
    base_.read({out, size});
    if (base_.gcount() != size) {
      return false;
    }
    return true;
  }

 private:
  template <typename T>
  std::optional<T> ReadScalar() {
    T result = T{};
    if (!RawRead(reinterpret_cast<char*>(&result), sizeof(result))) {
      return {};
    }
    return result;
  }

  base::ReadStream& base_;
};

}
}
