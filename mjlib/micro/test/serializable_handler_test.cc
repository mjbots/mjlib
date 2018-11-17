// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/micro/serializable_handler.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/visitor.h"

#include "mjlib/micro/event_queue.h"
#include "mjlib/micro/stream_pipe.h"

#include "mjlib/micro/test/reader.h"

using namespace mjlib::micro;
namespace base = mjlib::base;

namespace {
struct SubStruct {
  int detailed = 23;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(detailed));
  }
};

struct MyStruct {
  int int_value = 10;
  float float_value = 2.0;
  bool bool_value = false;
  SubStruct sub_value;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(int_value));
    a->Visit(MJ_NVP(float_value));
    a->Visit(MJ_NVP(bool_value));
    a->Visit(MJ_NVP(sub_value));
  }
};
}

BOOST_AUTO_TEST_CASE(BasicSerializableHandler) {
  MyStruct my_struct;

  SerializableHandler<MyStruct> dut(&my_struct);

  {
    char buffer[100] = {};
    base::BufferWriteStream write_stream{buffer};
    dut.WriteBinary(write_stream);
    BOOST_TEST(write_stream.offset() == 13);

    my_struct.int_value = 20;
    BOOST_TEST(my_struct.int_value == 20);

    base::BufferReadStream read_stream{{buffer, 13}};
    dut.ReadBinary(read_stream);
    BOOST_TEST(my_struct.int_value == 10);
  }

  {
    char buffer[1000] = {};
    base::BufferWriteStream write_stream{buffer};
    dut.WriteSchema(write_stream);
    BOOST_TEST(write_stream.offset() == 143);
  }

  {
    BOOST_TEST(my_struct.float_value == 2.0);
    dut.Set("float_value", "3.0");
    BOOST_TEST(my_struct.float_value == 3.0);
  }

  {
    char buffer[100] = {};
    EventQueue event_queue;
    StreamPipe stream_pipe{event_queue.MakePoster()};
    test::Reader reader{stream_pipe.side_b()};

    int complete_count = 0;
    const int result =
        dut.Read("sub_value.detailed", buffer, *stream_pipe.side_a(),
                 [&](base::error_code ec) {
                   BOOST_TEST(!ec);
                   complete_count++;
                 });
    BOOST_TEST(result == 0);
    BOOST_TEST(complete_count == 0);

    event_queue.Poll();
    BOOST_TEST(complete_count == 1);
    BOOST_TEST(reader.data_.str() == "23");
  }

  {
    BOOST_TEST(my_struct.float_value == 3.0);
    dut.SetDefault();
    BOOST_TEST(my_struct.float_value == 2.0);
  }
}
