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

#include "mjlib/io/async_sequence.h"

#include <deque>

#include <boost/asio/post.hpp>

#include "mjlib/base/assert.h"

namespace mjlib {
namespace io {

struct Item {
  ChainableCallback callback;
  std::string description;

  Item(ChainableCallback callback_in,
       std::string_view description_in)
      : callback(std::move(callback_in)),
        description(description_in) {}
};

class AsyncSequence::Impl : public std::enable_shared_from_this<Impl> {
 public:
  Impl(const boost::asio::any_io_executor& executor)
      : executor_(executor) {}

  void Start(ErrorCallback completion_callback) {
    MJ_ASSERT(!completion_callback_);
    completion_callback_ = std::move(completion_callback);
    RunNextOperation();
  }

  void RunNextOperation() {
    if (sequence_.empty()) {
      // Success!
      boost::asio::post(
          executor_, std::bind(std::move(completion_callback_), base::error_code()));
      return;
    }

    auto next_item = sequence_.front();
    sequence_.pop_front();

    boost::asio::post(
        executor_,
        [self=shared_from_this(), next_item]() {
          next_item->callback(
              std::bind(&Impl::HandleCallback,
                        self, std::placeholders::_1, next_item));
        });
  }

  void HandleCallback(base::error_code ec, std::shared_ptr<Item> item) {
    if (ec) {
      if (!item->description.empty()) {
        ec.Append("When executing: " + item->description);
      }
      boost::asio::post(
          executor_,
          std::bind(std::move(completion_callback_), ec));
      return;
    }

    RunNextOperation();
  }

  boost::asio::any_io_executor executor_;
  std::deque<std::shared_ptr<Item>> sequence_;
  ErrorCallback completion_callback_;
};

AsyncSequence::AsyncSequence(const boost::asio::any_io_executor& executor)
    : impl_(std::make_shared<Impl>(executor)) {}

AsyncSequence& AsyncSequence::Add(ChainableCallback callback,
                                  std::string_view description) {
  BOOST_ASSERT(!impl_->completion_callback_);
  impl_->sequence_.emplace_back(
      std::make_shared<Item>(std::move(callback), description));
  return *this;
}

void AsyncSequence::Start(ErrorCallback completion) {
  impl_->Start(std::move(completion));
}

}
}
