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

#include "mjlib/multiplex/frame.h"

#include <boost/crc.hpp>

#include "mjlib/base/crc_stream.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

std::string Frame::encode() const {
  base::FastOStringStream stream;
  encode(&stream);
  return std::string(stream.str());
}

void Frame::encode(base::WriteStream* stream) const {
  base::CrcWriteStream<boost::crc_ccitt_type> crc_stream{*stream};
  WriteStream writer{crc_stream};

  writer.Write<uint16_t>(Format::kHeader);
  writer.Write<uint8_t>(source_id | (request_reply ? 0x80 : 0x00));
  writer.Write<uint8_t>(dest_id);
  writer.WriteVaruint(payload.size());
  crc_stream.write(payload);
  const uint16_t checksum = crc_stream.checksum();
  writer.Write(checksum);
}

}
}
