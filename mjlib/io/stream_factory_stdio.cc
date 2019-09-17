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

#include "mjlib/io/stream_factory_stdio.h"

#include <functional>

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/post.hpp>

namespace mjlib {
namespace io {
namespace detail {

namespace {
using namespace std::placeholders;

class StdioStream : public AsyncStream {
 public:
  StdioStream(const boost::asio::executor& executor,
              const StreamFactory::Options& options)
      : executor_(executor),
        stdin_(executor, ::dup(options.stdio_in)),
        stdout_(executor, ::dup(options.stdio_out)) {
    BOOST_ASSERT(options.type == StreamFactory::Type::kStdio);
  }

  boost::asio::executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    stdin_.async_read_some(buffers, handler);
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    stdout_.async_write_some(buffers, handler);
  }

  void cancel() override {
    stdin_.cancel();
    stdout_.cancel();
  }

 private:
  boost::asio::executor executor_;
  boost::asio::posix::stream_descriptor stdin_;
  boost::asio::posix::stream_descriptor stdout_;
};
}

void AsyncCreateStdio(
    const boost::asio::executor& executor,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  boost::asio::post(
      executor,
      std::bind(handler, base::error_code(),
                std::make_shared<StdioStream>(executor, options)));
}

}
}
}
