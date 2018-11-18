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

#include "mjlib/micro/telemetry_manager.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/micro/async_exclusive.h"
#include "mjlib/micro/command_manager.h"
#include "mjlib/micro/event_queue.h"
#include "mjlib/micro/pool_ptr.h"
#include "mjlib/micro/stream_pipe.h"

using namespace mjlib::micro;

namespace {
struct Fixture {
  SizedPool<> pool;
  EventQueue event_queue;
  StreamPipe pipe{event_queue.MakePoster()};
  AsyncExclusive<AsyncWriteStream> write_stream{pipe.side_a()};
  CommandManager command_manager{&pool, pipe.side_a(), &write_stream};
  TelemetryManager dut{&pool, &command_manager, &write_stream};
};
}

BOOST_FIXTURE_TEST_CASE(BasicTelemetryManager, Fixture) {
}
