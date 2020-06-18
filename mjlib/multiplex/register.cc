// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

using BaseReadStream = multiplex::ReadStream<base::ReadStream>;
}

void RegisterRequest::ExpectResponse(bool value) {
  request_reply_ = value;
}

void RegisterRequest::ReadSingle(Register reg, size_t type_index) {
  WriteStream stream{buffer_};

  MJ_ASSERT(type_index <= 3);
  stream.WriteVaruint(u32(Format::Subframe::kReadBase) + type_index * 4 + 1);
  stream.WriteVaruint(reg);

  request_reply_ = true;
}

void RegisterRequest::ReadMultiple(Register reg, uint32_t num_registers,
                                   size_t type_index) {
  MJ_ASSERT(num_registers > 0);
  WriteStream stream{buffer_};

  MJ_ASSERT(type_index <= 3);
  const int encoded_length = (num_registers < 4) ? num_registers : 0;

  stream.WriteVaruint(u32(Format::Subframe::kReadBase) +
                      type_index * 4 + encoded_length);
  if (!encoded_length) { stream.WriteVaruint(num_registers); }
  stream.WriteVaruint(reg);

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
  WriteStream stream{buffer_};

  stream.WriteVaruint(u32(Format::Subframe::kWriteBase) + value.index() * 4 + 1);
  stream.WriteVaruint(reg);
  WriteValue(stream.base(), value);
}

void RegisterRequest::WriteMultiple(Register start_reg,
                                    const std::vector<Format::Value>& values) {
  MJ_ASSERT(!values.empty());
  WriteStream stream{buffer_};

  const int encoded_length = (values.size() < 4) ? values.size() : 0;

  stream.WriteVaruint(u32(Format::Subframe::kWriteBase) +
                      values.front().index() * 4 + encoded_length);
  if (!encoded_length) { stream.WriteVaruint(values.size()); }
  stream.WriteVaruint(start_reg);
  for (const auto& value : values) {
    WriteValue(stream.base(), value);
  }
}

std::string_view RegisterRequest::buffer() const {
  return std::string_view(buffer_.data()->data(), buffer_.data()->size());
}

void RegisterRequest::clear() {
  request_reply_ = false;
  buffer_.clear();
}

namespace {
std::optional<Format::Value> ReadValue(BaseReadStream& stream,
                                       size_t type_index) {
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

bool ParseSubframe(BaseReadStream& stream, std::vector<RegisterValue>* output) {
  const auto maybe_subframe_id = stream.ReadVaruint();
  if (!maybe_subframe_id) { return false; }
  const auto subframe_id = *maybe_subframe_id;

  if (subframe_id >= u32(Format::Subframe::kReplyBase) &&
      subframe_id < (u32(Format::Subframe::kReplyBase) + 16)) {
    const int encoded_length = subframe_id & 0x03;
    const int type =
        (subframe_id - u32(Format::Subframe::kReplyBase)) / 4;

    const auto maybe_num_registers =
        (encoded_length == 0) ? stream.ReadVaruint() :
        std::make_optional<uint32_t>(encoded_length);
    if (!maybe_num_registers) { return false; }
    const auto num_registers = *maybe_num_registers;

    const auto maybe_start_reg = stream.ReadVaruint();
    if (!maybe_start_reg) { return false; }
    const auto start_reg = *maybe_start_reg;

    for (size_t i = 0; i < num_registers; i++) {
      const auto maybe_value = ReadValue(stream, type);
      if (!maybe_value) { return false; }
      output->push_back(std::make_pair(start_reg + i, *maybe_value));
    }
  } else if (subframe_id == u32(Format::Subframe::kWriteError) ||
             subframe_id == u32(Format::Subframe::kReadError)) {
    const auto maybe_this_reg = stream.ReadVaruint();
    const auto maybe_this_err = stream.ReadVaruint();
    if (!maybe_this_reg || !maybe_this_err) {
      return false;
    }
    output->push_back(std::make_pair(*maybe_this_reg, *maybe_this_err));
  } else if (subframe_id == u32(Format::Subframe::kNop)) {
    return false;
  } else {
    // We could report an error someday.  For now, we'll just call
    // ourselves done.
    return false;
  }

  return true;
}
}

RegisterReply ParseRegisterReply(base::ReadStream& read_stream) {
  std::vector<RegisterValue> data;
  ParseRegisterReply(read_stream, &data);

  RegisterReply result;
  for (const auto& pair : data) {
    result.insert(pair);
  }

  return result;
}

void ParseRegisterReply(base::ReadStream& stream_in,
                        std::vector<RegisterValue>* result) {
  result->clear();
  BaseReadStream stream{stream_in};

  while (true) {
    const size_t old_size = result->size();
    const bool success = ParseSubframe(stream, result);
    if (!success) {
      result->resize(old_size);
      return;
    }
  }
}

}  // namespace multiplex
}  // namespace mjlib
