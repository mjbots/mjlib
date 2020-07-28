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

#pragma once

#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/aborting_posix_timer.h"

namespace mjlib {
namespace io {

/// This is an executor that can be used to verify real-time
/// performance of an event loop.  Individual events are timed, and a
/// posix signal is delivered if they run too long.  Additionally, a
/// posix signal is delivered if the event loop does not empty with
/// sufficient frequency.
///
/// It wraps an existing executor.
class RealtimeExecutor {
 public:
  using Base = boost::asio::io_context::executor_type;

  RealtimeExecutor(const Base& base) noexcept : base_(base) {}

  RealtimeExecutor(const RealtimeExecutor& other) noexcept : base_(other.base_) {}

  RealtimeExecutor(RealtimeExecutor&& other) noexcept
      : base_(std::move(other.base_)) {}

  ~RealtimeExecutor() noexcept {}


  struct Options {
    // Zero is disabled.
    int64_t event_timeout_ns = 0;
    int64_t idle_timeout_ns = 0;
  };

  // boost::asio::execution_context& context() { return base_.context(); }

  const boost::asio::io_context::executor_type& base() const {
    return base_;
  }

  // TODO(jpieper): I'm not fully comfortable with the property
  // mechanism.  However, this seems to work result in the desired
  // wrapping behavior, so I'm leaving it for now.
  template <typename T>
  RealtimeExecutor require(
      T value, typename std::add_pointer<
        decltype(std::declval<Base>().require(value))>::type = 0) const {
    return *this;
  }

  template <typename T>
  auto query(T value) const -> decltype(this->base().query(value)) {
    return base_.query(value);
  }

  template <typename Callback>
  void execute(Callback&& callback) const {
    Service& s = const_cast<Service&>(service());
    s.StartWork();
    base_.execute(Wrap<typename std::decay<Callback>::type>(
                      &s, std::move(callback)));
  }

  template <typename Callback, typename Allocator>
  void dispatch(Callback&& callback, const Allocator& a) {
    service().StartWork();
    base_.dispatch(Wrap<typename std::decay<Callback>::type>(
                       &service(), std::move(callback)), a);
  }

  template <typename Callback, typename Allocator>
  void post(Callback callback, const Allocator& a) {
    service().StartWork();
    base_.post(Wrap<typename std::decay<Callback>::type>(
                   &service(), std::move(callback)), a);
  }

  template <typename Callback, typename Allocator>
  void defer(Callback callback, const Allocator& a) {
    service().StartWork();
    base_.defer(Wrap<typename std::decay<Callback>::type>(
                    &service(), std::move(callback)), a);
  }

  void set_options(const Options& options) {
    service().options_ = options;
  }

  Options options() const {
    return service().options_;
  }

  friend bool operator==(const RealtimeExecutor& lhs,
                         const RealtimeExecutor& rhs) noexcept {
    return lhs.base_ == rhs.base_;
  }

 private:
  void on_work_started() {
    base_.on_work_started();
  }

  void on_work_finished() {
    base_.on_work_finished();
  }

  // We make this a templated class solely so that the static 'id'
  // member does not need a dedicated translation unit.
  template <typename Ignored>
  class TemplateService : public boost::asio::execution_context::service {
   public:
    TemplateService(boost::asio::execution_context& context)
        : boost::asio::execution_context::service(context) {}

    void shutdown() override {}

    void StartWork() {
      const auto original_work = outstanding_work_;
      outstanding_work_++;

      if (original_work == 0 &&
          options_.idle_timeout_ns) {
        // Start our timer to ensure that we get back to idle
        // sufficiently quickly.
        idle_timer_.Start(options_.idle_timeout_ns);
      }
    }

    void StopWork() {
      outstanding_work_--;
      if (outstanding_work_ == 0 &&
          options_.idle_timeout_ns) {
        // Stop our idle timer.
        idle_timer_.Stop();
      }
    }

    int outstanding_work_ = 0;

    Options options_;

    base::AbortingPosixTimer event_timer_{"Per event timer failed\n"};
    base::AbortingPosixTimer idle_timer_{"Event loop overload detected\n"};

    static boost::asio::execution_context::id id;
  };

  using Service = TemplateService<void>;

  template <typename Callback>
  class Wrap {
   public:
    Wrap(Service* service, Callback&& callback)
        : service_(service), callback_(std::move(callback)) {}

    void operator()() {
      if (service_->options_.event_timeout_ns) {
        service_->event_timer_.Start(service_->options_.event_timeout_ns);
      }

      callback_();

      if (service_->options_.event_timeout_ns) {
        service_->event_timer_.Stop();
      }
      service_->StopWork();
    }

    Service* service_ = nullptr;
    Callback callback_;
  };

  Service& service() {
    return boost::asio::use_service<Service>(base_.context());
  }

  const Service& service() const {
    return boost::asio::use_service<Service>(base_.context());
  }

  Base base_;
};

template <typename Ignored>
boost::asio::execution_context::id
RealtimeExecutor::TemplateService<Ignored>::id;

}
}
