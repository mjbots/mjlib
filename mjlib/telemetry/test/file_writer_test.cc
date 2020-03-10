// Copyright 2020 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/telemetry/file_writer.h"

#include <sstream>
#include <string>

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/temporary_file.h"

using mjlib::telemetry::FileWriter;

namespace {
std::string Contents(const std::string& filename) {
  std::ifstream inf(filename);
  std::ostringstream ostr;
  ostr << inf.rdbuf();
  return ostr.str();
}
}

BOOST_AUTO_TEST_CASE(FileWriterHeaderTest) {
  mjlib::base::TemporaryFile temp;

  {
    FileWriter dut;
    dut.Open(temp.native());
  }

  const auto contents = Contents(temp.native());
  BOOST_TEST(contents == std::string("TLOG0003\x00", 9));
}
