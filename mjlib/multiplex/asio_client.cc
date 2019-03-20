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

#include "mjlib/multiplex/asio_client.h"

#include "mjlib/io/exclusive_command.h"

namespace mjlib {
namespace multiplex {

class AsioClient::Impl {
 public:
  Impl(io::AsyncStream* stream, const Options& options)
      : stream_(stream),
        options_(options) {}

  void AsyncRegister(uint8_t id,
                     const RegisterRequest& request,
                     RegisterHandler handler) {
    lock_.Invoke([id, request](RegisterHandler handler) {
        // Create our full frame.

        // Send it.

        // Read a reply.
        handler(base::error_code(), RegisterReply());
      },
      handler);
  }

  io::SharedStream MakeTunnel(uint8_t, uint32_t, const TunnelOptions&) {
    return {};
  }

  io::AsyncStream* const stream_;
  const Options options_;

  io::ExclusiveCommand lock_{stream_->get_io_service()};
};

AsioClient::AsioClient(io::AsyncStream* stream, const Options& options)
    : impl_(std::make_unique<Impl>(stream, options)) {}

AsioClient::~AsioClient() {}

void AsioClient::AsyncRegister(uint8_t id,
                               const RegisterRequest& request,
                               RegisterHandler handler) {
  impl_->AsyncRegister(id, request, handler);
}

io::SharedStream AsioClient::MakeTunnel(
    uint8_t id, uint32_t channel, const TunnelOptions& options) {
  return impl_->MakeTunnel(id, channel, options);
}

}
}
