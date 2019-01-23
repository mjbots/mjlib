// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/visitor.h"

#include "mjlib/micro/async_exclusive.h"
#include "mjlib/micro/command_manager.h"
#include "mjlib/micro/event_queue.h"
#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/required_success.h"
#include "mjlib/micro/stream_pipe.h"

#include "mjlib/micro/test/reader.h"


namespace mjlib {
namespace micro {
namespace test {

struct MyData {
  int32_t value = 0;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(value));
  }
};

struct OtherData {
  int16_t stuff = 0;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(stuff));
  }
};

struct CommandManagerFixture {
  SizedPool<> pool;
  EventQueue event_queue;
  StreamPipe pipe{event_queue.MakePoster()};
  test::Reader reader{pipe.side_b()};
  AsyncExclusive<AsyncWriteStream> write_stream{pipe.side_a()};
  CommandManager command_manager{&pool, pipe.side_a(), &write_stream};

  MyData my_data;
  OtherData other_data;

  VoidCallback my_data_update;
  VoidCallback other_data_update;

  CommandManagerFixture() {
    command_manager.AsyncStart();
  }

  void ExpectResponse(const std::string_view& message) {
    BOOST_TEST(reader.data_.str() == message);
    reader.data_.str("");
  }

  void ExpectResponsePrefix(const std::string_view& message) {
    BOOST_TEST(reader.data_.str().substr(0, message.size()) == message);
    reader.data_.str("");
  }

  void Command(const std::string_view& message) {
    RequiredSuccess required_success;
    AsyncWrite(*pipe.side_b(), message, required_success.Make());
    event_queue.Poll();
  }
};

}
}
}
