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

#include <memory>

#include "mjlib/base/error_code.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/multiplex/register.h"

namespace mjlib {
namespace multiplex {

/// A client for the MultiplexProtocol based on boost::asio
class AsioClient {
 public:
  struct Options {
    uint8_t source_id = 0;

    Options() {}
  };
  AsioClient(io::AsyncStream*, const Options& = Options());
  ~AsioClient();

  using RegisterHandler = std::function<
    void (const base::error_code&, const RegisterReply&)>;

  /// Make a single register request.  The handler will always be
  /// invoked.  If a reply was requested, it will only be invoked on
  /// error or the reply.  If no reply was requested, it will be
  /// invoked once the write has been completed.
  void AsyncRegister(uint8_t id, const RegisterRequest&, RegisterHandler);

  struct TunnelOptions {
    // Poll this often for data to be received.
    double poll_rate_s = 0.01;

    TunnelOptions() {}
  };

  /// Allocate a tunnel which can be used to send and receive serial
  /// stream data.
  io::SharedStream MakeTunnel(
      uint8_t id,
      uint32_t channel,
      const TunnelOptions& options = TunnelOptions());

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
