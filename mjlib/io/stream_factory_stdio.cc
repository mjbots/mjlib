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

#include "mjlib/io/stream_factory_stdio.h"

#include <functional>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/post.hpp>

#include "fmt/format.h"

#include "mjlib/base/fail.h"
#include "mjlib/base/system_error.h"

namespace mjlib {
namespace io {
namespace detail {

namespace {
using namespace std::placeholders;

#ifndef _WIN32
class StdioStream : public AsyncStream {
 public:
  StdioStream(const boost::asio::any_io_executor& executor,
              const StreamFactory::Options& options)
      : executor_(executor),
        stdin_(executor, ::dup(options.stdio_in)),
        stdout_(executor, ::dup(options.stdio_out)) {
    BOOST_ASSERT(options.type == StreamFactory::Type::kStdio);
  }

  boost::asio::any_io_executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    stdin_.async_read_some(buffers, std::move(handler));
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    stdout_.async_write_some(buffers, std::move(handler));
  }

  void cancel() override {
    stdin_.cancel();
    stdout_.cancel();
  }

 private:
  boost::asio::any_io_executor executor_;
  boost::asio::posix::stream_descriptor stdin_;
  boost::asio::posix::stream_descriptor stdout_;
};
#else // _WIN32
class StdioStream : public AsyncStream {
 public:
  StdioStream(const boost::asio::any_io_executor& executor,
              const StreamFactory::Options& options)
     : executor_(executor),
       thread_(std::bind(&StdioStream::Run, this)) {
    BOOST_ASSERT(options.type == StreamFactory::Type::kStdio);
    base::system_error::throw_if(stdin_ == nullptr);
    base::system_error::throw_if(stdout_ == nullptr);
  }

  ~StdioStream() override {
    child_context_.stop();
    thread_.join();
  }

  boost::asio::any_io_executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    boost::asio::post(
      child_context_,
      [this, buffers, handler=std::move(handler)]() mutable {
        this->ChildRead(buffers, std::move(handler));
      }
    );
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    const auto& buffer = *buffers.begin();  
    DWORD bytes_written = 0;
    if (buffer.size() != 0) {
      base::system_error::throw_if(
        !WriteFile(stdout_, buffer.data(), buffer.size(), &bytes_written, nullptr));
    }
    boost::asio::post(
      executor_,
      std::bind(std::move(handler), base::error_code(), bytes_written)
    );
  }

  void cancel() override {
  }

 private:
  void Run() {
    boost::asio::io_context::work work(child_context_);
    child_context_.run();
  }

  void ChildRead(MutableBufferSequence buffers,
                 ReadHandler handler) {
    auto& buffer = *buffers.begin();
    DWORD bytes_read = 0;
    if (buffer.size() != 0) {
      base::system_error::throw_if(
        !ReadConsole(stdin_, buffer.data(), buffer.size(), &bytes_read, nullptr));
    }

    boost::asio::post(
      executor_,
      std::bind(std::move(handler), base::error_code(), bytes_read)
    );
  }

  boost::asio::any_io_executor executor_;

  std::thread thread_;
  boost::asio::io_context child_context_;
  HANDLE stdin_{GetStdHandle(STD_INPUT_HANDLE)};
  HANDLE stdout_{GetStdHandle(STD_OUTPUT_HANDLE)};
};
#endif
}

void AsyncCreateStdio(
    const boost::asio::any_io_executor& executor,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  boost::asio::post(
      executor,
      std::bind(std::move(handler), base::error_code(),
                std::make_shared<StdioStream>(executor, options)));
}

}
}
}
