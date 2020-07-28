// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/io/async_types.h"
#include "mjlib/multiplex/frame.h"

namespace mjlib {
namespace multiplex {

class FrameStream {
 public:
  virtual ~FrameStream() {}

  struct Properties {
    int max_size = -1;
  };

  virtual Properties properties() const = 0;

  virtual void AsyncWrite(const Frame*, io::ErrorCallback) = 0;
  virtual void AsyncWriteMultiple(const std::vector<const Frame*>&,
                                  io::ErrorCallback) = 0;

  /// If @p timeout is not-special, @p callback will be invoked with
  /// boost::asio::error::operation_aborted after that much time has
  /// elapsed.
  virtual void AsyncRead(Frame*, boost::posix_time::time_duration timeout,
                         io::ErrorCallback callback) = 0;

  /// Cancel any outstanding operations.
  virtual void cancel() = 0;

    /// @return true if there is data available to be read.  This means
  /// an AsyncRead *may* be able to complete without touching the
  /// operating system.
  virtual bool read_data_queued() const = 0;

  virtual boost::asio::any_io_executor get_executor() const = 0;
};

}
}
