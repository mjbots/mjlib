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

#include <memory>

#include <boost/asio/execution_context.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/system/error_code.hpp>

#include "mjlib/base/system_error.h"
#include "mjlib/io/timer_selector.h"

namespace mjlib {
namespace io {

class DeadlineTimer {
 public:
  using duration_type = TimerBase::duration_type;
  using time_type = TimerBase::time_type;
  using traits_type = TimerBase::traits_type;
  using executor_type = TimerBase::executor_type;

  DeadlineTimer(boost::asio::io_context& context)
      : executor_(context.get_executor()) {}
  DeadlineTimer(const boost::asio::any_io_executor& executor)
      : executor_(executor) {}
  ~DeadlineTimer() {}

  void async_wait(ErrorCallback handler) {
    make_delegate()->async_wait(std::move(handler));
  }

  void wait() {
    make_delegate()->wait();
  }

  std::size_t cancel() {
    boost::system::error_code ec;
    return make_delegate()->cancel(ec);
    if (ec) { throw mjlib::base::system_error(ec); }
  }

  std::size_t cancel(boost::system::error_code& ec) {
    return make_delegate()->cancel(ec);
  }

  std::size_t cancel_one() {
    boost::system::error_code ec;
    return make_delegate()->cancel_one(ec);
    if (ec) { throw mjlib::base::system_error(ec); }
  }

  std::size_t cancel_one(boost::system::error_code& ec) {
    return make_delegate()->cancel_one(ec);
  }

  time_type expires_at() const {
    return make_delegate()->expires_at();
  }

  std::size_t expires_at(const time_type& expiry_time) {
    boost::system::error_code ec;
    auto result = make_delegate()->expires_at(expiry_time, ec);
    if (ec) { throw mjlib::base::system_error(ec); }
    return result;
  }

  std::size_t expires_at(const time_type& expiry_time,
                         boost::system::error_code& ec) {
    return make_delegate()->expires_at(expiry_time, ec);
  }

  std::size_t expires_from_now(const duration_type& expiry_time) {
    boost::system::error_code ec;
    auto result = make_delegate()->expires_from_now(expiry_time, ec);
    if (ec) { throw mjlib::base::system_error(ec); }
    return result;
  }

  std::size_t expires_from_now(const duration_type& expiry_time,
                               boost::system::error_code& ec) {
    return make_delegate()->expires_from_now(expiry_time, ec);
  }

  executor_type get_executor() {
    return make_delegate()->get_executor();
  }

 private:
  TimerBase* make_delegate() {
    if (!timer_base_) {
      timer_base_ =
          boost::asio::use_service<TimerSelector>(executor_.context())
          .construct(executor_);
    }
    return timer_base_.get();
  }

  const TimerBase* make_delegate() const {
    if (!timer_base_) {
      timer_base_ =
          boost::asio::use_service<TimerSelector>(executor_.context())
          .construct(executor_);
    }
    return timer_base_.get();
  }

  TimerBase::executor_type executor_;
  mutable std::unique_ptr<TimerBase> timer_base_;
};

}
}
