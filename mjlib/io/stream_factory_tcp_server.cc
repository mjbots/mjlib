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

#include "mjlib/io/stream_factory_tcp_server.h"

#include <boost/asio/ip/tcp.hpp>

namespace mjlib {
namespace io {
namespace detail {
namespace pl = std::placeholders;
using tcp = boost::asio::ip::tcp;

namespace {
class TcpServerStream : public AsyncStream {
 public:
  TcpServerStream(const boost::asio::any_io_executor& executor,
                  const StreamFactory::Options& options)
      : executor_(executor),
        options_(options),
        acceptor_(executor),
        socket_(executor) {
    tcp::endpoint endpoint(tcp::v4(), options_.tcp_server_port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    Accept();
  }

  ~TcpServerStream() override {}

  void Accept() {
    acceptor_.async_accept(
        socket_,
        std::bind(&TcpServerStream::HandleAccept, this, pl::_1));
  }

  boost::asio::any_io_executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    BOOST_ASSERT(started_);

    read_buffers_ = buffers;
    read_handler_ = std::move(handler);

    if (connected_) {
      socket_.async_read_some(
          buffers, std::bind(&TcpServerStream::HandleRead, this, pl::_1, pl::_2));
    } else {
      read_queued_ = true;
    }
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    BOOST_ASSERT(started_);

    write_buffers_ = buffers;
    write_handler_ = std::move(handler);

    if (connected_) {
      socket_.async_write_some(
          buffers, std::bind(&TcpServerStream::HandleWrite, this, pl::_1, pl::_2));
    } else {
      write_queued_ = true;
    }
  }

  void cancel() override {
    BOOST_ASSERT(started_);

    if (connected_) {
      socket_.cancel();
    } else {
      if (read_queued_) {
        read_queued_ = false;
        boost::asio::post(
            executor_,
            std::bind(std::move(read_handler_),
                      boost::asio::error::operation_aborted, 0));
      }
      if (write_queued_) {
        write_queued_ = false;
        boost::asio::post(
            executor_,
            std::bind(std::move(write_handler_),
                      boost::asio::error::operation_aborted, 0));
      }
    }
  }

  ErrorCallback start_handler_;

 private:
  void HandleAccept(const base::error_code& ec) {
    if (!started_) {
      boost::asio::post(
          executor_,
          std::bind(std::move(start_handler_), ec));
      started_ = true;
    }
    connected_ = true;

    if (read_queued_) {
      read_queued_ = false;
      async_read_some(read_buffers_, std::move(read_handler_));
    }
    if (write_queued_) {
      write_queued_ = false;
      async_write_some(write_buffers_, std::move(write_handler_));
    }
  }

  void HandleRead(const base::error_code& ec,
                  std::size_t size) {
    if (ec == boost::asio::error::eof) {
      read_queued_ = true;
      connected_ = false;
      socket_.close();
      Accept();
    } else {
      boost::asio::post(
          executor_,
          std::bind(std::move(read_handler_), ec, size));
    }
  }

  void HandleWrite(const base::error_code& ec,
                   std::size_t size) {
    if (ec == boost::asio::error::eof) {
      write_queued_ = true;
      connected_ = false;
      socket_.close();
      Accept();
    } else {
      boost::asio::post(
          executor_,
          std::bind(std::move(write_handler_), ec, size));
    }
  }

  boost::asio::any_io_executor executor_;
  const StreamFactory::Options options_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  bool started_ = false;
  bool connected_ = false;

  MutableBufferSequence read_buffers_;
  ReadHandler read_handler_;
  bool read_queued_ = false;

  ConstBufferSequence write_buffers_;
  WriteHandler write_handler_;
  bool write_queued_ = false;
};
}

void AsyncCreateTcpServer(
    const boost::asio::any_io_executor& executor,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  auto stream = std::make_shared<TcpServerStream>(executor, options);
  stream->start_handler_ = std::bind(std::move(handler), pl::_1, stream);
}

}
}
}
