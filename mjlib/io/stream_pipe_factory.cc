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

#include "mjlib/io/stream_pipe_factory.h"

#include <map>
#include <optional>

#include <boost/asio/post.hpp>

namespace mjlib {
namespace io {

namespace {
class HalfPipe : public AsyncStream {
 public:
  HalfPipe(const boost::asio::any_io_executor& executor)
      : executor_(executor) {}
  ~HalfPipe() override {}

  void SetOther(HalfPipe* other) { other_ = other; }

  boost::asio::any_io_executor get_executor() override { return executor_; }
  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    BOOST_ASSERT(other_);
    BOOST_ASSERT(!read_handler_);

    if (boost::asio::buffer_size(buffers) == 0) {
      // Post immediately.
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), base::error_code(), 0));
      return;
    }

    if (other_->write_handler_) {
      const std::size_t written =
          boost::asio::buffer_copy(buffers, *other_->write_buffers_);
      boost::asio::post(
          executor_,
          std::bind(std::move(*other_->write_handler_),
                    base::error_code(), written));
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), base::error_code(), written));

      other_->write_handler_ = {};
      other_->write_buffers_ = {};
    } else {
      read_buffers_ = std::move(buffers);
      read_handler_ = std::move(handler);
    }
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    BOOST_ASSERT(other_);
    BOOST_ASSERT(!write_handler_);

    if (boost::asio::buffer_size(buffers) == 0) {
      // Post immediately.
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), base::error_code(), 0));
      return;
    }

    if (other_->read_handler_) {
      const std::size_t written =
          boost::asio::buffer_copy(*other_->read_buffers_, buffers);
      boost::asio::post(
          executor_,
          std::bind(std::move(*other_->read_handler_),
                    base::error_code(), written));
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), base::error_code(), written));

      other_->read_handler_ = {};
      other_->read_buffers_ = {};
    } else {
      write_buffers_ = std::move(buffers);
      write_handler_ = std::move(handler);
    }
  }

  void cancel() override {
    if (read_handler_) {
      boost::asio::post(
          executor_,
          std::bind(std::move(*read_handler_),
                    boost::asio::error::operation_aborted, 0));
      read_handler_ = {};
      read_buffers_ = {};
    }

    if (write_handler_) {
      boost::asio::post(
          executor_,
          std::bind(std::move(*write_handler_),
                    boost::asio::error::operation_aborted, 0));
      write_handler_ = {};
      write_buffers_ = {};
    }
  }

 private:
  boost::asio::any_io_executor executor_;
  HalfPipe* other_ = nullptr;

  std::optional<MutableBufferSequence> read_buffers_;
  std::optional<ReadHandler> read_handler_;

  std::optional<ConstBufferSequence> write_buffers_;
  std::optional<WriteHandler> write_handler_;
};

class BidirectionalPipe : boost::noncopyable {
 public:
  BidirectionalPipe(const boost::asio::any_io_executor& executor)
      : direction_a_(executor),
        direction_b_(executor) {
    direction_a_.SetOther(&direction_b_);
    direction_b_.SetOther(&direction_a_);
  }

  HalfPipe direction_a_;
  HalfPipe direction_b_;
};

/// A reference to a HalfPipe which also maintains the shared pointer
/// ownership of its parent.  This ensures that the parent stays
/// around as long as any callers have a stream.
class HalfPipeRef : public AsyncStream {
 public:
  HalfPipeRef(std::shared_ptr<BidirectionalPipe> parent,
              HalfPipe* pipe)
      : parent_(parent),
        pipe_(pipe) {}

  boost::asio::any_io_executor get_executor() override {
    return pipe_->get_executor();
  }

  void cancel() override { pipe_->cancel(); }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    return pipe_->async_read_some(buffers, std::move(handler));
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    return pipe_->async_write_some(buffers, std::move(handler));
  }

 private:
  std::shared_ptr<BidirectionalPipe> parent_;
  HalfPipe* const pipe_;
};

}

class StreamPipeFactory::Impl {
 public:
  Impl(const boost::asio::any_io_executor& executor) : executor_(executor) {}

  boost::asio::any_io_executor executor_;
  std::map<std::string, std::shared_ptr<BidirectionalPipe>> pipes_;
};

StreamPipeFactory::StreamPipeFactory(const boost::asio::any_io_executor& executor)
    : impl_(std::make_unique<Impl>(executor)) {}

StreamPipeFactory::~StreamPipeFactory() {}

SharedStream StreamPipeFactory::GetStream(const std::string& key, int direction) {
  if (impl_->pipes_.count(key) == 0) {
    impl_->pipes_.insert(
        std::make_pair(key,
                       std::make_shared<BidirectionalPipe>(impl_->executor_)));
  }

  auto parent = impl_->pipes_[key];
  return std::make_shared<HalfPipeRef>(
      parent, (direction == 0) ? &parent->direction_a_ : &parent->direction_b_);
}

}
}
