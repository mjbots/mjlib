// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/thread_writer.h"

#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/temporary_file.h"

namespace fs = boost::filesystem;

using mjlib::base::ThreadWriter;

BOOST_AUTO_TEST_CASE(BasicThreadWriterTest) {
  mjlib::base::TemporaryFile temp;
  {
    ThreadWriter dut{temp.native()};
    {
      auto buf = std::make_unique<ThreadWriter::OStream>();
      buf->write("test");
      dut.Write(std::move(buf));
    }
    {
      auto buf = std::make_unique<ThreadWriter::OStream>();
      buf->write("more");
      dut.Write(std::move(buf));
    }
  }

  std::ifstream inf(temp.native());
  std::ostringstream ostr;
  ostr << inf.rdbuf();
  BOOST_TEST(ostr.str() == "testmore");
}
