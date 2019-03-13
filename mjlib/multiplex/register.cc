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
#include "mjlib/base/fail.h"

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

void RegisterRequest::WriteSingle(Register reg, Format::Value value) {
  stream_.WriteVaruint(u32(Format::Subframe::kWriteSingleBase) + value.index());
  stream_.WriteVaruint(reg);
  WriteValue(stream_.base(), value);
}

void RegisterRequest::WriteMultiple(Register start_reg,
                                    const std::vector<Format::Value>& values) {
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

namespace {
std::optional<Format::Value> ReadValue(ReadStream& stream, size_t type_index) {
  MJ_ASSERT(type_index <= 3);
  if (type_index == 0) {
    return stream.Read<int8_t>();
  } else if (type_index == 1) {
    return stream.Read<int16_t>();
  } else if (type_index == 2) {
    return stream.Read<int32_t>();
  } else if (type_index == 3) {
    return stream.Read<float>();
  }
  base::AssertNotReached();
}

std::optional<RegisterReply> ParseSubframe(ReadStream& stream) {
  const auto maybe_subframe_id = stream.ReadVaruint();
  if (!maybe_subframe_id) { return {}; }
  const auto subframe_id = *maybe_subframe_id;

  RegisterReply this_result;

  if ((subframe_id & ~0x03) == u32(Format::Subframe::kReplySingleBase)) {
    const auto maybe_this_reg = stream.ReadVaruint();
    if (!maybe_this_reg) { return {}; }

    const auto maybe_value = ReadValue(stream, subframe_id & 0x03);
    if (!maybe_value) { return {}; }

    this_result[*maybe_this_reg] = *maybe_value;
  } else if ((subframe_id & ~0x03) == u32(Format::Subframe::kReplyMultipleBase)) {
    const auto maybe_start_reg = stream.ReadVaruint();
    if (!maybe_start_reg) { return {}; }
    const auto start_reg = *maybe_start_reg;

    const auto maybe_num_registers = stream.ReadVaruint();
    if (!maybe_num_registers) { return {}; }
    const auto num_registers = *maybe_num_registers;

    for (size_t i = 0; i < num_registers; i++) {
      const auto maybe_value = ReadValue(stream, subframe_id & 0x03);
      if (!maybe_value) { return {}; }
      this_result[start_reg + i] = *maybe_value;
    }
  } else if (subframe_id == u32(Format::Subframe::kWriteError) ||
             subframe_id == u32(Format::Subframe::kReadError)) {
    const auto maybe_this_reg = stream.ReadVaruint();
    const auto maybe_this_err = stream.ReadVaruint();
    if (!maybe_this_reg || !maybe_this_err) {
      return {};
    }
    this_result[*maybe_this_reg] = *maybe_this_err;
  } else {
    // We could report an error someday.  For now, we'll just call
    // ourselves done.
    return {};
  }

  return this_result;
}
}

RegisterReply ParseRegisterReply(base::ReadStream& read_stream) {
  multiplex::ReadStream stream{read_stream};

  RegisterReply result;
  while (true) {
    const auto maybe_result = ParseSubframe(stream);
    if (!maybe_result) { return result; }

    // Merge the current result into our final.
    for (const auto& pair : *maybe_result) {
      result.insert(pair);
    }
  }
}

}
}
