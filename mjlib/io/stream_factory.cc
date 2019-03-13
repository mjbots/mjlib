// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/io/stream_factory.h"

#include "mjlib/io/stream_factory_stdio.h"

namespace mjlib {
namespace io {

std::map<StreamFactory::Type, const char*> StreamFactory::TypeMapper() {
  return {
    { Type::kStdio, "stdio" },
    { Type::kSerial, "serial" },
    { Type::kTcpClient, "tcp" },
    { Type::kTcpServer, "tcp_server" },
    { Type::kPipe, "pipe" },
  };
}

class StreamFactory::Impl {
 public:
  Impl(boost::asio::io_service& service) : service_(service) {}

  boost::asio::io_service& service_;
};

StreamFactory::StreamFactory(boost::asio::io_service& service)
    : impl_(std::make_unique<Impl>(service)) {}

StreamFactory::~StreamFactory() {}

void StreamFactory::AsyncCreate(const Options& options, StreamHandler handler) {
  switch (options.type) {
    case Type::kStdio: {
      detail::AsyncCreateStdio(impl_->service_, options, handler);
      return;
    }
    case Type::kSerial:
    case Type::kTcpClient:
    case Type::kTcpServer:
    case Type::kPipe: {
      BOOST_ASSERT(false);
      return;
    }
  }
}

}
}
