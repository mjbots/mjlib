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

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/io/async_stream.h"
#include "mjlib/io/async_types.h"
#include "mjlib/multiplex/frame.h"

namespace mjlib {
namespace multiplex {

/// Writes and reads a set of multiplex frames over a stream
/// connection.
class FrameStream {
 public:
  FrameStream(io::AsyncStream*);
  ~FrameStream();

  void AsyncWrite(const Frame*, io::ErrorCallback);

  /// If @p timeout is not-special, @p callback will be invoked with
  /// boost::asio::error::operation_aborted after that much time has
  /// elapsed.
  void AsyncRead(Frame*, boost::posix_time::time_duration timeout,
                 io::ErrorCallback callback);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
