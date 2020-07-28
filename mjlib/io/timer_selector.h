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

#include <functional>
#include <memory>

#include <boost/asio/basic_deadline_timer.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/time_traits.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/system/error_code.hpp>

#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

class TimerBase {
 public:
  using duration_type = boost::posix_time::time_duration;
  using time_type = boost::posix_time::ptime;
  using traits_type = boost::asio::time_traits<time_type>;
  using executor_type = boost::asio::any_io_executor;

  virtual ~TimerBase() {};

  virtual std::size_t cancel(boost::system::error_code&) = 0;
  virtual std::size_t cancel_one(boost::system::error_code&) = 0;
  virtual time_type expires_at() const = 0;
  virtual std::size_t expires_at(const time_type&,
                                 boost::system::error_code&) =0;
  virtual duration_type expires_from_now() const = 0;
  virtual std::size_t expires_from_now(const duration_type&,
                                       boost::system::error_code&) = 0;
  virtual void async_wait(ErrorCallback) = 0;
  virtual void wait() = 0;
  virtual executor_type get_executor() = 0;
};

class AsioTimer : public TimerBase {
 public:
  AsioTimer(executor_type executor) : timer_(executor) {}
  ~AsioTimer() override {}

  std::size_t cancel(boost::system::error_code& ec) override {
    return timer_.cancel(ec);
  }

  std::size_t cancel_one(boost::system::error_code& ec) override {
    return timer_.cancel_one(ec);
  }

  time_type expires_at() const override {
    return timer_.expires_at();
  }

  std::size_t expires_at(const time_type& time,
                         boost::system::error_code& ec) override {
    return timer_.expires_at(time, ec);
  }

  duration_type expires_from_now() const override {
    return timer_.expires_from_now();
  }

  std::size_t expires_from_now(const duration_type& time,
                               boost::system::error_code& ec) override {
    return timer_.expires_from_now(time, ec);
  }

  void async_wait(ErrorCallback callback) override {
    return timer_.async_wait(std::move(callback));
  }

  void wait() override {
    return timer_.wait();
  }

  executor_type get_executor() override {
    return timer_.get_executor();
  }

 private:
  boost::asio::basic_deadline_timer<
    time_type,
    traits_type,
    executor_type> timer_;
};

class TimerSelector : public boost::asio::execution_context::service {
 public:
  using executor_type = TimerBase::executor_type;

  TimerSelector(boost::asio::execution_context& context)
      : boost::asio::execution_context::service(context) {}

  void shutdown() override {
    factory_ = {};
    now_getter_ = {};
  }

  std::unique_ptr<TimerBase> construct(executor_type executor) {
    return factory_(executor);
  }

  TimerBase::time_type now() {
    return now_getter_();
  }

  using Factory = std::function<std::unique_ptr<TimerBase>(executor_type)>;
  using NowGetter = std::function<TimerBase::time_type()>;

  void Reset(Factory new_factory, NowGetter now_getter) {
    factory_ = new_factory;
    now_getter_ = now_getter;
  }

  static boost::asio::execution_context::id id;

 private:
  Factory factory_ = [](executor_type executor) {
    return std::make_unique<AsioTimer>(executor);
  };
  NowGetter now_getter_ = []() {
    return boost::posix_time::microsec_clock::universal_time();
  };
};

}
}
