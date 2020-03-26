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

#include "mjlib/base/aborting_posix_timer.h"

#include <unistd.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv) {
  po::options_description desc;

  int64_t delay_us = 0;
  int64_t sleep_us = 0;

  desc.add_options()
      ("help,h", "display usage message")
      ("delay-us", po::value(&delay_us), "make timer delay this much")
      ("sleep-us", po::value(&sleep_us), "sleep this much")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return 0;
  }

  mjlib::base::AbortingPosixTimer dut("my test message\n");
  dut.Start(delay_us * 1000);
  ::usleep(sleep_us);
  dut.Stop();
  ::usleep(sleep_us);
  std::cout << "Exiting without failure\n";
  return 0;
}
