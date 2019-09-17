// Copyright 2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/base/assert.h"

namespace mjlib {
namespace io {

struct Item {
  ChainableCallback callback;
  std::string description;

  Item(ChainableCallback callback_in,
       std::string_view description_in)
      : callback(callback_in),
        description(description_in) {}
};

class AsyncSequence::Impl : public std::enable_shared_from_this<Impl> {
 public:
  Impl(boost::asio::io_context& service)
      : service_(service) {}

  void Start(ErrorCallback completion_callback) {
    MJ_ASSERT(!completion_callback_);
    completion_callback_ = completion_callback;
    RunNextOperation();
  }

  void RunNextOperation() {
    if (sequence_.empty()) {
      // Success!
      service_.post(std::bind(completion_callback_, base::error_code()));
      return;
    }

    auto next_item = sequence_.front();
    sequence_.pop_front();

    service_.post([self=shared_from_this(), next_item]() {
        next_item.callback(
            std::bind(&Impl::HandleCallback,
                      self, std::placeholders::_1, next_item));
      });
  }

  void HandleCallback(base::error_code ec, const Item& item) {
    if (ec) {
      if (!item.description.empty()) {
        ec.Append("When executing: " + item.description);
      }
      service_.post(std::bind(completion_callback_, ec));
      return;
    }

    RunNextOperation();
  }

  boost::asio::io_context& service_;
  std::deque<Item> sequence_;
  ErrorCallback completion_callback_;
};

AsyncSequence::AsyncSequence(boost::asio::io_context& service)
    : impl_(std::make_shared<Impl>(service)) {}

AsyncSequence& AsyncSequence::op(ChainableCallback callback,
                                 std::string_view description) {
  BOOST_ASSERT(!impl_->completion_callback_);
  impl_->sequence_.emplace_back(callback, description);
  return *this;
}

void AsyncSequence::Start(ErrorCallback completion) {
  impl_->Start(completion);
}

}
}
