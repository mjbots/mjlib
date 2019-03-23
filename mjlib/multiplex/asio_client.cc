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

#include <functional>

#include <boost/asio/write.hpp>
#include <boost/crc.hpp>

#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/io/exclusive_command.h"

#include "mjlib/multiplex/frame_stream.h"
#include "mjlib/multiplex/stream.h"

namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {

class AsioClient::Impl {
 public:
  Impl(io::AsyncStream* stream, const Options& options)
      : stream_(stream),
        options_(options),
        frame_stream_(stream_) {}

  void AsyncRegister(uint8_t id,
                     const RegisterRequest& request,
                     RegisterHandler handler) {
    lock_.Invoke([this, id, request](RegisterHandler handler) {
        tx_frame_.source_id = this->options_.source_id;
        tx_frame_.dest_id = id;
        tx_frame_.request_reply = request.request_reply();
        tx_frame_.payload = request.buffer();

        frame_stream_.AsyncWrite(
            &tx_frame_, std::bind(&Impl::HandleWrite, this, pl::_1,
                                  handler, request.request_reply()));
      },
      handler);
  }

  void HandleWrite(const base::error_code& ec,
                   RegisterHandler handler,
                   bool request_reply) {
    if (!request_reply) {
      stream_->get_io_service().post(std::bind(handler, ec, RegisterReply()));
      return;
    }

    FailIf(ec);

    const boost::posix_time::time_duration kDefaultTimeout =
        boost::posix_time::milliseconds(10);

    frame_stream_.AsyncRead(
        &rx_frame_, kDefaultTimeout,
        std::bind(&Impl::HandleRead, this, pl::_1, handler));
  }

  void HandleRead(const base::error_code& ec, RegisterHandler handler) {
    base::FailIf(ec);

    base::FastIStringStream stream(rx_frame_.payload);
    auto reply = ParseRegisterReply(stream);
    stream_->get_io_service().post(
        std::bind(handler, ec, reply));
  }

  io::SharedStream MakeTunnel(uint8_t, uint32_t, const TunnelOptions&) {
    return {};
  }

 private:
  io::AsyncStream* const stream_;
  const Options options_;
  FrameStream frame_stream_;

  io::ExclusiveCommand lock_{stream_->get_io_service()};

  Frame rx_frame_;
  Frame tx_frame_;
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
