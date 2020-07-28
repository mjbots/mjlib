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

#include <boost/asio/any_io_executor.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <function2/function2.hpp>

#include "mjlib/base/error_code.h"
#include "mjlib/io/async_types.h"
#include "mjlib/io/deadline_timer.h"

namespace mjlib {
namespace io {

/// This timer class attempts to invoke its callback at a fixed rate.
/// It has cycle skip protection and will skip cycles if they have
/// been missed entirely.  Every group of skipped cycles will invoke
/// the callback at least once with a
/// boost::asio::error::operation_aborted.
class RepeatingTimer {
 public:
  RepeatingTimer(const boost::asio::any_io_executor&);

  using Callback = fu2::function<void (const mjlib::base::error_code&)>;

  // Unlike most boost::asio callbacks, @p callback is invoked
  // possibly many times, at a regular interval.
  void start(boost::posix_time::time_duration, Callback callback);

  // Stop invoking the callback.  Return if anything was actually
  // cancelled.  If the timer callback has already been enqueued, the
  // callback may still be invoked with a success argument.
  size_t cancel();

 private:
  void StartInternal();
  void HandleTimer(const base::error_code&);

  boost::asio::any_io_executor executor_;
  DeadlineTimer timer_;
  boost::posix_time::time_duration period_;
  Callback callback_;
};

}
}
