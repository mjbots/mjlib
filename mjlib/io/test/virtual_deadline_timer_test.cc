// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/io/deadline_timer.h"

#include <boost/asio/error.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/system_error.h"
#include "mjlib/io/timer_selector.h"

using namespace mjlib;

BOOST_AUTO_TEST_CASE(BasicVirtualDeadlineTimer) {
  boost::asio::io_context context;
  io::DeadlineTimer timer(context);
  timer.expires_from_now(boost::posix_time::milliseconds(1));
  timer.wait();
}

namespace {

// A TimerBase whose cancel/cancel_one always report an error via the
// boost::system::error_code out-parameter. Used to verify that the
// no-arg cancel() and cancel_one() overloads on DeadlineTimer
// surface that error rather than silently swallowing it.
class CancelErrorTimer : public io::TimerBase {
 public:
  CancelErrorTimer(executor_type executor) : executor_(executor) {}
  ~CancelErrorTimer() override {}

  std::size_t cancel(boost::system::error_code& ec) override {
    ec = boost::asio::error::operation_aborted;
    return 0;
  }
  std::size_t cancel_one(boost::system::error_code& ec) override {
    ec = boost::asio::error::operation_aborted;
    return 0;
  }
  time_type expires_at() const override { return {}; }
  std::size_t expires_at(const time_type&,
                         boost::system::error_code&) override { return 0; }
  duration_type expires_from_now() const override { return {}; }
  std::size_t expires_from_now(const duration_type&,
                               boost::system::error_code&) override {
    return 0;
  }
  void async_wait(io::ErrorCallback) override {}
  void wait() override {}
  executor_type get_executor() override { return executor_; }

 private:
  executor_type executor_;
};

}  // namespace

BOOST_AUTO_TEST_CASE(CancelThrowsOnError) {
  boost::asio::io_context context;
  boost::asio::use_service<io::TimerSelector>(context).Reset(
      [](io::TimerBase::executor_type executor) {
        return std::make_unique<CancelErrorTimer>(executor);
      },
      []() { return boost::posix_time::ptime(); });

  io::DeadlineTimer timer(context);
  BOOST_CHECK_THROW(timer.cancel(), base::system_error);
  BOOST_CHECK_THROW(timer.cancel_one(), base::system_error);
}
