// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#include <array>
#include <string>

#include "mjlib/base/fail.h"
#include "mjlib/base/system_error.h"

namespace mjlib {
namespace base {

/// Wraps a POSIX real-time timer.  When it expires, a message is
/// printed to stderr followed by aborting the program.  This can be
/// used for diagnostics or real time assurance.
class AbortingPosixTimer {
 public:
  AbortingPosixTimer(std::string_view message)
      : context_(MakeContext()) {
    struct sigevent se = {};
    // We'll use SIGEV_THREAD_ID to make this library class less
    // intrusive.  Only the thread which instantiated it will receive
    // the signals.
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = context_.signal_number;

    context_.message = std::string(message);
    // We call c_str() now, because we can't do it in the signal
    // handler.
    context_.message_ptr = context_.message.c_str();
    context_.message_len = context_.message.size();

    system_error::throw_if(
        ::timer_create(CLOCK_MONOTONIC, &se, &timer_id_) < 0);

    struct sigaction sa = {};
    sa.sa_handler = &AbortingPosixTimer::Handler;
    system_error::throw_if(
        ::sigaction(context_.signal_number, &sa, nullptr) < 0);
  }

  ~AbortingPosixTimer() {
    Stop();

    struct sigaction sa = {};
    sa.sa_handler = SIG_DFL;
    system_error::throw_if(
        ::sigaction(context_.signal_number, &sa, nullptr) < 0);

    system_error::throw_if(::timer_delete(timer_id_));

    context_ = {};
  }

  /// Start the timer.  If it expires before Stop is called, then the
  /// process will be aborted.
  void Start(int64_t nanoseconds) {
    struct itimerspec it = {};

    auto& ts = it.it_value;
    ts.tv_sec = nanoseconds / 1000000000;
    ts.tv_nsec = nanoseconds % 1000000000;

    system_error::throw_if(::timer_settime(timer_id_, 0, &it, nullptr) < 0);
  }

  /// Cancel the timer.
  void Stop() {
    struct itimerspec it = {};

    system_error::throw_if(::timer_settime(timer_id_, 0, &it, nullptr) < 0);
  }

 private:
  struct Context {
    int signal_number = -1;
    std::string message;
    const char* message_ptr;
    size_t message_len = 0;
  };

  static void Handler(int signo) {
    // We are in a signal handler.  There is almost nothing we can do
    // here.  Almost all library and system calls are off limits,
    // including memory allocation.  Write out a pre-allocated
    // message, then abort the program.
    [&]() {
      for (auto& context : contexts_) {
        if (signo == context.signal_number) {
          (void)::write(2, context.message_ptr, context.message_len);
          break;
        }
      }
    }();

    ::abort();
  }

  static int FindUnusedSignal() {
    // Look through the available real time signals and find one that
    // has no handler installed.
    for (int i = SIGRTMIN; i < SIGRTMAX; i++) {
      struct sigaction sa = {};
      system_error::throw_if(::sigaction(i, nullptr, &sa) < 0);
      if (sa.sa_handler == SIG_DFL) { return i; }
    }
    Fail("Exhausted available signals");
  }

  static Context& MakeContext() {
    for (auto& context : contexts_) {
      if (context.signal_number < 0) {
        context.signal_number = FindUnusedSignal();
        return context;
      }
    }
    Fail("Exhausted signal contexts");
  }

  static constexpr int kMaxTimers = 16;

  static std::array<Context, kMaxTimers> contexts_;

  timer_t timer_id_ = {};
  Context& context_;
};

}
}
