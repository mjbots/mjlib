// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <memory>

#include <boost/asio/any_io_executor.hpp>

#include "mjlib/io/async_stream.h"

namespace mjlib {
namespace io {

class StreamPipeFactory {
 public:
  StreamPipeFactory(const boost::asio::any_io_executor&);
  ~StreamPipeFactory();

  SharedStream GetStream(const std::string& key, int direction);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
