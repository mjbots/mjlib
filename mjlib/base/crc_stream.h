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

#include "mjlib/base/stream.h"

namespace mjlib {
namespace base {

/// A ReadStream wrapper which calculates the CRC of all bytes read /
/// ignored.
template <typename CrcType>
class CrcReadStream : public ReadStream {
 public:
  CrcReadStream(ReadStream& base) : base_(base) {}

  using result_type = typename CrcType::value_type;

  void ignore(std::streamsize size) override {
    data_.resize(size);
    base_.read(string_span(&data_[0], size));
    crc_.process_bytes(&data_[0], base_.gcount());
  }

  void read(const string_span& data) override {
    base_.read(data);
    crc_.process_bytes(data.data(), base_.gcount());
  }

  std::streamsize gcount() const override {
    return base_.gcount();
  }

  result_type checksum() const {
    return crc_.checksum();
  }

  CrcType& crc() { return crc_; }

 private:
  ReadStream& base_;
  CrcType crc_;
  std::vector<char> data_;
};

/// A WriteStream wrapper that checksums everything being written to
/// it.
template <typename CrcType>
class CrcWriteStream : public WriteStream {
 public:
  CrcWriteStream(WriteStream& base) : base_(base) {}

  using result_type = typename CrcType::value_type;

  void write(const std::string_view& data) override {
    base_.write(data);
    crc_.process_bytes(data.data(), data.size());
  }

  result_type checksum() const {
    return crc_.checksum();
  }

 private:
  WriteStream& base_;
  CrcType crc_;
};

}
}
