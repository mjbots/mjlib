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

#include <cstdint>
#include <string>

#include "mjlib/base/stream.h"

namespace mjlib {
namespace multiplex {

struct Frame {
  Frame() {}
  Frame(uint8_t source_id_in,
        bool request_reply_in,
        uint8_t dest_id_in,
        const std::string& payload_in)
      : source_id(source_id_in),
        request_reply(request_reply_in),
        dest_id(dest_id_in),
        payload(payload_in) {}

  void encode(base::WriteStream*) const;
  std::string encode() const;

  uint8_t source_id = 0;
  bool request_reply = false;
  uint8_t dest_id = 0;
  std::string payload;
};

}
}
