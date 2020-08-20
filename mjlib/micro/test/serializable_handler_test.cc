// Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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
  int32_t detailed = 23;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(detailed));
  }
};

struct MyStruct {
  int32_t int_value = 10;
  float float_value = 2.0;
  bool bool_value = false;
  SubStruct sub_value;
  std::array<float, 3> array_value = {6.0, 7.0, 8.0};
  std::array<SubStruct, 2> array_struct = { {} };
  uint32_t u32_value = 10;
  int64_t i64_value = std::numeric_limits<int64_t>::min();
  uint64_t u64_value = std::numeric_limits<uint64_t>::max();
  std::optional<int32_t> optional_unset_int;
  std::optional<int32_t> optional_set_int = 4;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(int_value));
    a->Visit(MJ_NVP(float_value));
    a->Visit(MJ_NVP(bool_value));
    a->Visit(MJ_NVP(sub_value));
    a->Visit(MJ_NVP(array_value));
    a->Visit(MJ_NVP(array_struct));
    a->Visit(MJ_NVP(u32_value));
    a->Visit(MJ_NVP(i64_value));
    a->Visit(MJ_NVP(u64_value));
    a->Visit(MJ_NVP(optional_unset_int));
    a->Visit(MJ_NVP(optional_set_int));
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
    BOOST_TEST(write_stream.offset() == 59);

    my_struct.int_value = 20;
    BOOST_TEST(my_struct.int_value == 20);

    base::BufferReadStream read_stream{{buffer, 39}};
    dut.ReadBinary(read_stream);
    BOOST_TEST(my_struct.int_value == 10);
  }

  {
    char buffer[1000] = {};
    base::BufferWriteStream write_stream{buffer};
    dut.WriteSchema(write_stream);
    BOOST_TEST(write_stream.offset() == 308);
  }

  {
    BOOST_TEST(my_struct.float_value == 2.0);
    dut.Set("float_value", "3.0");
    BOOST_TEST(my_struct.float_value == 3.0);

    BOOST_TEST(my_struct.array_value[2] == 8.0);
    dut.Set("array_value.2", "2.0");
    BOOST_TEST(my_struct.array_value[2] == 2.0);

    BOOST_TEST(!my_struct.optional_unset_int);
    dut.Set("optional_unset_int", "9");
    BOOST_TEST(!!my_struct.optional_unset_int);
    BOOST_TEST(*my_struct.optional_unset_int == 9);
  }

  {
    BOOST_TEST(my_struct.i64_value == std::numeric_limits<int64_t>::min());
    dut.Set("i64_value", "9223372036854775807"); // INT64_MAX
    BOOST_TEST(my_struct.i64_value == std::numeric_limits<int64_t>::max());
  }

  {
    BOOST_TEST(my_struct.u64_value == std::numeric_limits<uint64_t>::max());
    dut.Set("u64_value", "18446744073709551614");
    BOOST_TEST(my_struct.u64_value ==
               (std::numeric_limits<uint64_t>::max() - 1));
  }

  {
    char buffer[100] = {};
    EventQueue event_queue;
    StreamPipe stream_pipe{event_queue.MakePoster()};
    test::Reader reader{stream_pipe.side_b()};

    int complete_count = 0;
    {
      const int result =
          dut.Read("sub_value.detailed", buffer, *stream_pipe.side_a(),
                   [&](error_code ec) {
                     BOOST_TEST(!ec);
                     complete_count++;
                 });
      BOOST_TEST(result == 0);
    }
    BOOST_TEST(complete_count == 0);

    event_queue.Poll();
    BOOST_TEST(complete_count == 1);
    BOOST_TEST(reader.data_.str() == "23");
    reader.data_.str("");

    {
      const int result =
          dut.Read("array_value.1", buffer, *stream_pipe.side_a(),
                   [&](error_code ec) {
                     BOOST_TEST(!ec);
                     complete_count++;
                   });
      BOOST_TEST(result == 0);
    }

    event_queue.Poll();
    BOOST_TEST(complete_count == 2);

    BOOST_TEST(reader.data_.str() == "7.000000");
  }

  {
    BOOST_TEST(my_struct.float_value == 3.0);
    dut.SetDefault();
    BOOST_TEST(my_struct.float_value == 2.0);
  }
  {
    const int result = dut.Set("array_struct.1.detailed", "10");
    BOOST_TEST(result == 0);
    BOOST_TEST(my_struct.array_struct[1].detailed == 10);
  }
  {
    const int result = dut.Set("array_struct.1.foo", "10");
    BOOST_TEST(result != 0);
  }
  {
    const int result = dut.Set("array_struct.10.detailed", "10");
    BOOST_TEST(result != 0);
  }
  {
    const int result = dut.Set("array_struct.bing.detailed", "20");
    BOOST_TEST(result != 0);
  }

  auto test_read = [&](std::string_view key, std::string_view expected) {
    char buffer[100] = {};
    EventQueue event_queue;
    StreamPipe stream_pipe{event_queue.MakePoster()};
    test::Reader reader{stream_pipe.side_b()};

    int complete_count = 0;
    {
      const int result =
          dut.Read(key, buffer, *stream_pipe.side_a(),
                   [&](error_code ec) {
                     BOOST_TEST(!ec);
                     complete_count++;
                 });
      BOOST_TEST(result == 0);
    }

    event_queue.Poll();
    BOOST_TEST(complete_count == 1);
    BOOST_TEST(reader.data_.str() == expected);
  };

  test_read("u32_value", "10");
  test_read("optional_unset_int", "");
  test_read("optional_set_int", "4");
}

BOOST_AUTO_TEST_CASE(EnumerateTest) {
  EventQueue event_queue;
  StreamPipe stream_pipe{event_queue.MakePoster()};
  test::Reader reader{stream_pipe.side_b()};

  char buffer[100] = {};

  MyStruct my_struct;

  SerializableHandler<MyStruct> dut(&my_struct);
  detail::EnumerateArchive::Context context;

  int done_count = 0;
  error_code done_ec;

  dut.Enumerate(&context,
                buffer,
                "prefix",
                *stream_pipe.side_a(),
                [&](error_code ec) {
                  done_count++;
                  done_ec = ec;
                });

  BOOST_TEST(done_count == 0);
  event_queue.Poll();
  BOOST_TEST(done_count == 1);
  BOOST_TEST(done_ec == error_code());
  const std::string expected =
      "prefix.int_value 10\r\n"
      "prefix.float_value 2.000000\r\n"
      "prefix.bool_value 0\r\n"
      "prefix.sub_value.detailed 23\r\n"
      "prefix.array_value.0 6.000000\r\n"
      "prefix.array_value.1 7.000000\r\n"
      "prefix.array_value.2 8.000000\r\n"
      "prefix.array_struct.0.detailed 23\r\n"
      "prefix.array_struct.1.detailed 23\r\n"
      "prefix.u32_value 10\r\n"
      "prefix.i64_value -9223372036854775808\r\n"
      "prefix.u64_value 18446744073709551615\r\n"
      "prefix.optional_unset_int \r\n"
      "prefix.optional_set_int 4\r\n"
      ;

  BOOST_TEST(reader.data_.str() == expected);
}
