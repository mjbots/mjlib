// Copyright 2015-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/io/stream_factory_serial.h"

#include <boost/asio/post.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <fmt/format.h>

namespace mjlib {
namespace io {
namespace detail {

namespace {
namespace pl = std::placeholders;
using tcp = boost::asio::ip::tcp;

class TcpStream : public AsyncStream {
 public:
  TcpStream(const boost::asio::any_io_executor& executor,
            const StreamFactory::Options& options)
      : executor_(executor),
        options_(options),
        resolver_(executor),
        socket_(executor) {
    tcp::resolver::query query(options.tcp_target,
                               fmt::format("{}", options.tcp_target_port));
    resolver_.async_resolve(
        query, std::bind(&TcpStream::HandleResolve, this, pl::_1, pl::_2));
  }

  ~TcpStream() override {}

  boost::asio::any_io_executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    socket_.async_read_some(buffers, std::move(handler));
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    socket_.async_write_some(buffers, std::move(handler));
  }

  void cancel() override {
    resolver_.cancel();
    socket_.cancel();
  }

  ErrorCallback start_handler_;

 private:
  void HandleResolve(base::error_code ec,
                     tcp::resolver::iterator it) {
    if (ec) {
      ec.Append(fmt::format("when resolving: {}:{}",
                            options_.tcp_target, options_.tcp_target_port));
      boost::asio::post(
          executor_,
          std::bind(std::move(start_handler_), ec));
      return;
    }

    socket_.async_connect(*it, std::bind(&TcpStream::HandleConnect, this, pl::_1));
  }

  void HandleConnect(base::error_code ec) {
    if (ec) {
      ec.Append(fmt::format("when connecting to: {}:{}",
                            options_.tcp_target, options_.tcp_target_port));
    }
    boost::asio::post(
        executor_,
        std::bind(std::move(start_handler_), ec));
  }

  boost::asio::any_io_executor executor_;
  const StreamFactory::Options options_;
  tcp::resolver resolver_;
  tcp::socket socket_;
};

}

void AsyncCreateTcpClient(
    const boost::asio::any_io_executor& executor,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  auto stream = std::make_shared<TcpStream>(executor, options);
  stream->start_handler_ = std::bind(std::move(handler), pl::_1, stream);
}

}
}
}
