// Copyright 2016-2020 Josh Pieper, jjp@pobox.com.
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

#include <map>

#include <boost/asio/post.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/fail.h"
#include "mjlib/io/async_types.h"
#include "mjlib/io/timer_selector.h"

namespace mjlib {
namespace io {

class DebugDeadlineService {
 public:
  static DebugDeadlineService* Install(boost::asio::io_context& context) {
    auto& timer_selector = boost::asio::use_service<TimerSelector>(context);
    auto debug_service = std::make_shared<DebugDeadlineService>();

    timer_selector.Reset([debug_service](TimerBase::executor_type executor) {
        return std::make_unique<Timer>(debug_service.get(), executor);
      },
      [debug_service]() {
        return debug_service->now();
      });
    return debug_service.get();
  }

  boost::posix_time::ptime now() const {
    return current_time_;
  }

  void SetTime(boost::posix_time::ptime new_time) {
    current_time_ = new_time;

    while (!queue_.empty() && queue_.begin()->first <= current_time_) {
      auto& entry = queue_.begin()->second;
      auto& item = *entry.item;

      item.position_ = queue_.end();
      boost::asio::post(
          item.executor_,
          std::bind(std::move(entry.handler), boost::system::error_code()));
      queue_.erase(queue_.begin());
    }
  }

 private:
  class Timer;

  struct Entry {
    Timer* item = nullptr;
    io::ErrorCallback handler;
  };

  typedef std::multimap<boost::posix_time::ptime, Entry> Queue;
  class Timer : public TimerBase {
   public:
    Timer(DebugDeadlineService* debug_service, executor_type executor)
        : debug_service_(debug_service),
          executor_(executor),
          position_(debug_service->queue_.end()) {}
    ~Timer() override {}

    std::size_t cancel(boost::system::error_code& ec) override {
      ec = {};

      auto& queue = debug_service_->queue_;

      std::size_t result = 0;
      if (position_ != queue.end()) {
        result++;
        auto& entry = position_->second;
        MJ_ASSERT(this == entry.item);
        boost::asio::post(
            executor_,
            std::bind(std::move(entry.handler),
                      boost::asio::error::operation_aborted));
        queue.erase(position_);
        position_ = queue.end();
      }
      return result;
    }

    std::size_t cancel_one(boost::system::error_code& ec) override {
      return cancel(ec);
    }

    boost::posix_time::ptime expires_at() const override {
      return timestamp_;
    }

    std::size_t expires_at(const time_type& timestamp,
                           boost::system::error_code& ec) override {
      std::size_t result = cancel(ec);
      if (ec) { return result; }

      timestamp_ = timestamp;
      return result;
    }

    boost::posix_time::time_duration expires_from_now() const override {
      return timestamp_ - debug_service_->now();
    }

    std::size_t expires_from_now(
        const duration_type& duration,
        boost::system::error_code& ec) override {
      return expires_at(debug_service_->now() + duration, ec);
    }

    void async_wait(ErrorCallback handler) override {
      Entry entry;
      entry.item = this;
      entry.handler = std::move(handler);
      position_ = debug_service_->queue_.insert(
          std::make_pair(timestamp_, std::move(entry)));
    }

    void wait() override {
      base::Fail("not implemented");
    }

    executor_type get_executor() override {
      return executor_;
    }

    DebugDeadlineService* const debug_service_;
    boost::asio::any_io_executor executor_;
    boost::posix_time::ptime timestamp_;
    Queue::iterator position_;
  };

  boost::posix_time::ptime current_time_;
  Queue queue_;
};

}
}
