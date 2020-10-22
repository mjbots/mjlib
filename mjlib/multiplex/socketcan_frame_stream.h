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

#include "mjlib/base/visitor.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/io/async_types.h"
#include "mjlib/multiplex/frame_stream.h"
#include "mjlib/multiplex/frame.h"

namespace mjlib {
namespace multiplex {

/// Writes and reads a set of multiplex frames over a socketcan
/// interface.
class SocketcanFrameStream : public FrameStream {
 public:
  struct Options {
    std::string interface = "vcan0";

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(interface));
    }
  };

  SocketcanFrameStream(const boost::asio::any_io_executor&, const Options&);
  ~SocketcanFrameStream() override;

  Properties properties() const override;

  void AsyncStart(io::ErrorCallback);

  void AsyncWrite(const Frame*, io::ErrorCallback) override;

  void AsyncWriteMultiple(const std::vector<const Frame*>&,
                          io::ErrorCallback) override;

  void AsyncRead(Frame*, boost::posix_time::time_duration timeout,
                 io::ErrorCallback callback) override;

  void cancel() override;

  bool read_data_queued() const override;

  boost::asio::any_io_executor get_executor() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
