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

#include <functional>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>

#include "mjlib/io/selector.h"
#include "mjlib/io/stream_factory.h"

#include "mjlib/multiplex/stream_asio_client.h"
#include "mjlib/multiplex/fdcanusb_frame_stream.h"
#include "mjlib/multiplex/frame_stream.h"
#include "mjlib/multiplex/rs485_frame_stream.h"
#include "mjlib/multiplex/socketcan_frame_stream.h"

namespace mjlib {
namespace multiplex {

class StreamAsioClientBuilder : public AsioClient {
 public:
  struct Options {
    StreamAsioClient::Options client;
    io::StreamFactory::Options stream;
    SocketcanFrameStream::Options can;
    std::string frame_type = "fdcanusb";

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(client));
      a->Visit(MJ_NVP(stream));
      a->Visit(MJ_NVP(can));
      a->Visit(MJ_NVP(frame_type));
    }
  };

  StreamAsioClientBuilder(const boost::asio::any_io_executor& executor,
                          const Options& options)
      : executor_(executor),
        options_(options) {
    frame_stream_selector_.Register<FdcanusbFrameStream>("fdcanusb");
    frame_stream_selector_.Register<Rs485FrameStream>("rs485");
    frame_stream_selector_.set_default("fdcanusb");
  }

  ~StreamAsioClientBuilder() override {}

  io::Selector<FrameStream, io::AsyncStream*>* selector() {
    return &frame_stream_selector_;
  }

  void AsyncStart(mjlib::io::ErrorCallback callback) {
    if (options_.frame_type == "socketcan") {
      frame_stream_ = std::make_unique<SocketcanFrameStream>(executor_, options_.can);
      client_.emplace(frame_stream_.get());
      boost::asio::post(
          executor_,
          std::bind(std::move(callback), base::error_code()));
    } else {
      factory_.AsyncCreate(
          options_.stream, [this, callback = std::move(callback)](
              auto&& _1, auto&& _2) mutable {
            this->HandleStream(_1, _2, std::move(callback));
          });
    }
  }

  void AsyncTransmit(const Request* request,
                     Reply* reply,
                     io::ErrorCallback callback) override {
    client_->AsyncTransmit(request, reply, std::move(callback));
  }

  io::SharedStream MakeTunnel(
      uint8_t id,
      uint32_t channel,
      const TunnelOptions& options) override {
    return client_->MakeTunnel(id, channel, options);
  }

 private:
  void HandleStream(const base::error_code& ec, io::SharedStream stream,
                    io::ErrorCallback callback) {
    if (ec) {
      boost::asio::post(
          executor_,
          std::bind(std::move(callback), ec));
      return;
    }

    stream_ = stream;

    frame_stream_selector_.set_default(options_.frame_type);
    frame_stream_selector_.AsyncStart(
        [this, callback = std::move(callback)](auto&& _1) mutable {
          this->HandleFrameStream(_1, std::move(callback));
        },
        stream_.get());
  }

  void HandleFrameStream(const base::error_code& ec, io::ErrorCallback callback) {
    if (ec) {
      boost::asio::post(
          executor_,
          std::bind(std::move(callback), ec));
      return;
    }

    client_.emplace(frame_stream_selector_.selected());
    boost::asio::post(
        executor_,
        std::bind(std::move(callback), base::error_code()));
  }

  boost::asio::any_io_executor executor_;
  const Options options_;
  io::StreamFactory factory_{executor_};
  io::SharedStream stream_;
  io::Selector<FrameStream, io::AsyncStream*> frame_stream_selector_{
    executor_, "frame_type"};
  std::unique_ptr<FrameStream> frame_stream_;
  std::optional<StreamAsioClient> client_;
};

}
}
