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

#include <memory>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/base/error_code.h"
#include "mjlib/base/visitor.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/multiplex/asio_client.h"
#include "mjlib/multiplex/frame_stream.h"
#include "mjlib/multiplex/register.h"

namespace mjlib {
namespace multiplex {

/// A client for the MultiplexProtocol based on boost::asio which uses
/// FrameStream.
class StreamAsioClient : public AsioClient {
 public:
  struct Options {
    uint8_t source_id = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(source_id));
    }

    Options() {}
  };
  StreamAsioClient(FrameStream*, const Options& = Options());
  ~StreamAsioClient();

  void AsyncTransmit(const Request*,
                     Reply*,
                     io::ErrorCallback) override;

  /// Allocate a tunnel which can be used to send and receive serial
  /// stream data.
  io::SharedStream MakeTunnel(
      uint8_t id,
      uint32_t channel,
      const TunnelOptions& options = TunnelOptions()) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
