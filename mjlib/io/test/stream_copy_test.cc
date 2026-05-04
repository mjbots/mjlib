// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/io/stream_copy.h"

#include <memory>
#include <optional>

#include <boost/test/auto_unit_test.hpp>

#include <boost/asio/io_context.hpp>

#include "mjlib/io/stream_pipe_factory.h"

using namespace mjlib::io;
namespace base = mjlib::base;

// Demonstrates that destroying a BidirectionalStreamCopy in the done
// callback corrupts the still-pending operation on the other
// direction.
BOOST_AUTO_TEST_CASE(BidirectionalStreamCopyDestroyInCallback) {
  boost::asio::io_context context;
  StreamPipeFactory factory(context.get_executor());

  // The user-facing pair: 'left'/'right' that the copy will tunnel.
  auto user_left = factory.GetStream("user", 0);
  auto user_right = factory.GetStream("user", 1);

  // The "remote" pair: 'remote_a'/'remote_b'.  We will plumb the copy
  // between user_left and remote_a, and between user_right and
  // remote_b — that is the typical use-case (two real streams piped
  // through one logical stream).  But the simplest demonstration just
  // needs two independent streams.
  auto stream_left = factory.GetStream("loop", 0);
  auto stream_right = factory.GetStream("loop", 1);

  std::optional<BidirectionalStreamCopy> copy;
  base::error_code seen_ec;
  bool callback_fired = false;
  copy.emplace(
      context.get_executor(),
      stream_left.get(), stream_right.get(),
      [&](const base::error_code& ec) {
        callback_fired = true;
        seen_ec = ec;
        // A user might destroy the copy here.  This is the realistic
        // pattern for "we got the done callback, we're finished, tear
        // down."
        copy.reset();
      });

  // Pump the executor briefly so the initial async_read_some calls
  // are armed on each HalfPipe.
  context.poll();
  context.restart();

  // Now cancel only stream_left.  Cancelling the left HalfPipe will
  // post operation_aborted to the read_handler currently held by
  // copy1_ (which is reading from stream_left).  copy2_ is reading
  // from stream_right and is still pending and untouched.
  stream_left->cancel();

  // Drive the executor.  The cancelled read fires, posts the
  // BidirectionalStreamCopy::HandleDone, which posts the user
  // callback, which resets the optional and destroys copy1_+copy2_.
  context.run();

  BOOST_TEST(callback_fired == true);

  // At this point copy2_ is destroyed but its read_handler_ is still
  // held by stream_right's HalfPipe, with the bound `this` pointing
  // at the destroyed copy2_ memory.  Triggering it now invokes
  // StreamCopy::HandleRead on a dangling `this`.
  context.restart();
  stream_right->cancel();
  context.run();
}
