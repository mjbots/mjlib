// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/base/crc.h"

#include <boost/crc.hpp>

namespace mjlib {
namespace base {

uint32_t CalculateCrc(const std::string_view& data) {
  boost::crc_32_type crc;
  crc.process_bytes(data.data(), data.size());
  return crc.checksum();
}

}
}
