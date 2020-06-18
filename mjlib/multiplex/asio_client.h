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

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <function2/function2.hpp>

#include "mjlib/base/error_code.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/multiplex/register.h"

namespace mjlib {
namespace multiplex {

class AsioClient {
 public:
  virtual ~AsioClient() {};

  struct IdRequest {
    uint8_t id = 0;
    RegisterRequest request;
  };

  using Request = std::vector<IdRequest>;

  struct IdRegisterValue {
    uint8_t id = 0;
    Format::Register reg;
    Format::ReadResult value;
  };

  using Reply = std::vector<IdRegisterValue>;

  /// Make a register request.  The handler will always be
  /// invoked.  If a reply was requested, it will only be invoked on
  /// error or the reply.  If no reply was requested, it will be
  /// invoked once the write has been completed.
  ///
  /// Both @p request and @p reply must remain valid until the
  /// callback is invoked.
  virtual void AsyncTransmit(const Request* request,
                             Reply* reply,
                             io::ErrorCallback) = 0;

  struct TunnelOptions {
    // Poll this often for data to be received.
    boost::posix_time::time_duration poll_rate =
        boost::posix_time::milliseconds(10);

    TunnelOptions() {}
  };

  /// Allocate a tunnel which can be used to send and receive serial
  /// stream data.
  virtual io::SharedStream MakeTunnel(
      uint8_t id,
      uint32_t channel,
      const TunnelOptions& options = TunnelOptions()) = 0;
};

}
}
