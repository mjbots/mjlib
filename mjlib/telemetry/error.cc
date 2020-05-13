// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/telemetry/error.h"

namespace mjlib {
namespace telemetry {

namespace {
class ErrorCategory : public boost::system::error_category {
 public:
  const char* name() const noexcept override {
    return "mjlib.telemetry";
  }

  std::string message(int ev) const override {
    switch (static_cast<errc>(ev)) {
      case errc::kInvalidType: return "Invalid type";
      case errc::kInvalidUnionIndex: return "Invalid union index";
      case errc::kInvalidHeader: return "Invalid header";
      case errc::kInvalidBlockType: return "Invalid block type";
      case errc::kInvalidHeaderFlags: return "Invalid header flags";
      case errc::kUnknownBlockDataFlag: return "Unknown block data flag";
      case errc::kUnknownBlockSchemaFlag: return "Unknown block schema flag";
      case errc::kUnknownIndexFlag: return "Unkown index flag";
      case errc::kUnknownSeekMarkerFlag: return "Unknown seek marker flag";
      case errc::kDataChecksumMismatch: return "Data checksum mismatch";
      case errc::kDecompressionError: return "Decompression error";
      case errc::kTypeMismatch: return "Type mismatch";
    }
    return "unknown";
  }
};

const ErrorCategory& make_error_category() {
  static ErrorCategory result;
  return result;
}

}  // namespace

boost::system::error_code make_error_code(errc err) {
  return boost::system::error_code(static_cast<int>(err), make_error_category());
}

}
}
