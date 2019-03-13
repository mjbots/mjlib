// Copyright 2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/multiplex/register.h"

#include "mjlib/base/assert.h"

#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

namespace {
template <typename T>
uint32_t u32(T value) {
  return static_cast<uint32_t>(value);
}
}

void RegisterRequest::ExpectResponse(bool value) {
  request_reply_ = value;
}

void RegisterRequest::ReadSingle(Register reg, size_t type_index) {
  MJ_ASSERT(type_index <= 3);
  stream_.WriteVaruint(u32(Format::Subframe::kReadSingleBase) + type_index);
  stream_.WriteVaruint(reg);

  request_reply_ = true;
}

void RegisterRequest::ReadMultiple(Register reg, uint32_t num_registers,
                                   size_t type_index) {
  MJ_ASSERT(type_index <= 3);
  stream_.WriteVaruint(u32(Format::Subframe::kReadMultipleBase) + type_index);
  stream_.WriteVaruint(reg);
  stream_.WriteVaruint(num_registers);

  request_reply_ = true;
}

namespace {
void WriteValue(base::WriteStream* stream, const Format::Value& value) {
  std::visit([&](auto element) {
      stream->write(
          std::string_view(
              reinterpret_cast<const char*>(&element), sizeof(element)));
    }, value);
}
}

void RegisterRequest::WriteSingle(Register reg, Value value) {
  stream_.WriteVaruint(u32(Format::Subframe::kWriteSingleBase) + value.index());
  stream_.WriteVaruint(reg);
  WriteValue(stream_.base(), value);
}

void RegisterRequest::WriteMultiple(Register start_reg,
                                    const std::vector<Value>& values) {
  MJ_ASSERT(!values.empty());
  stream_.WriteVaruint(u32(Format::Subframe::kWriteMultipleBase) +
                       values.front().index());
  stream_.WriteVaruint(start_reg);
  stream_.WriteVaruint(values.size());
  for (const auto& value : values) {
    WriteValue(stream_.base(), value);
  }
}

std::string_view RegisterRequest::buffer() const {
  return std::string_view(buffer_.data()->data(), buffer_.data()->size());
}

}
}
