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

#pragma once

#include <map>

#include "mjlib/base/fast_stream.h"
#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

/// Build up a request to read or write from one or more registers.
class RegisterRequest {
 public:
  using Register = Format::Register;
  using Value = Format::Value;

  /// By default a response is only requested if a read operation is
  /// made.  If you wish to have a response even for write-only
  /// operations (so as to see errors), set this to true.
  void ExpectResponse(bool);

  void ReadSingle(Register, size_t type_index);
  void ReadMultiple(Register start_register, uint32_t num_registers,
                    size_t type_index);

  void WriteSingle(Register, Value);
  void WriteMultiple(Register, const std::vector<Value>&);

  std::string_view buffer() const;
  bool request_reply() const { return request_reply_; }

  void clear();

 private:
  base::FastOStringStream buffer_;
  bool request_reply_ = false;
};

/// The possible reply to a register operation.
using RegisterReply = std::map<Format::Register, Format::ReadResult>;

RegisterReply ParseRegisterReply(base::ReadStream&);

using RegisterValue = std::pair<Format::Register, Format::ReadResult>;

/// This version of the above call can be used in a system where no
/// steady state memory allocation is required.  @p result is not
/// required to be empty initially, and no operation will be performed
/// which causes it to shrink (it may grow if necessary)
void ParseRegisterReply(base::ReadStream&,
                        std::vector<RegisterValue>* result);

}
}
