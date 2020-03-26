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

#include "mjlib/io/realtime_executor.h"

#include <boost/program_options.hpp>

#include "mjlib/io/repeating_timer.h"

namespace po = boost::program_options;

int main(int argc, char** argv) {
  po::options_description desc;

  int timer_period_ms = 100;
  int timer_sleep_ms = 10;

  int64_t realtime_event_ms = 0;
  int64_t realtime_idle_ms = 0;

  desc.add_options()
      ("help,h", "display usage message")
      ("timer-period-ms", po::value(&timer_period_ms))
      ("timer-sleep-ms", po::value(&timer_sleep_ms))
      ("realtime-event-ms", po::value(&realtime_event_ms))
      ("realtime-idle-ms", po::value(&realtime_idle_ms))
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return 0;
  }

  boost::asio::io_context context;
  mjlib::io::RealtimeExecutor dut{context.get_executor()};
  dut.set_options([&]() {
      mjlib::io::RealtimeExecutor::Options options;
      options.event_timeout_ns = realtime_event_ms * 1000000;
      options.idle_timeout_ns = realtime_idle_ms * 1000000;
      return options;
    }());

  int count = 0;

  mjlib::io::RepeatingTimer timer1(dut);
  timer1.start(
      boost::posix_time::milliseconds(timer_period_ms),
      [&](auto ec) {
        if (ec == boost::asio::error::operation_aborted) { return; }
        std::cout << "timer1 " << count++ << "\n";

        ::usleep(timer_sleep_ms * 1000);
      });

  mjlib::io::RepeatingTimer timer2(dut);
  timer2.start(
      boost::posix_time::milliseconds(timer_period_ms),
      [&](auto ec) {
        if (ec == boost::asio::error::operation_aborted) { return; }
        std::cout << "timer2 " << count++ << "\n";

        ::usleep(timer_sleep_ms * 1000);
      });

  context.run();

  return 0;
}
