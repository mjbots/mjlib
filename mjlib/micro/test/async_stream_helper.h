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

#include "mjlib/micro/async_stream.h"

namespace mjlib {
namespace micro {
namespace test {

class DutStream : public AsyncStream {
 public:
  void AsyncReadSome(const base::string_span& data,
                     const SizeCallback& cbk) override {
    read_data_ = data;
    read_cbk_ = cbk;
    read_count_++;
  }

  void AsyncWriteSome(const std::string_view& data,
                      const SizeCallback& cbk) override {
    write_data_ = data;
    write_cbk_ = cbk;
    write_count_++;
  }

  base::string_span read_data_;
  SizeCallback read_cbk_;
  int read_count_ = 0;

  std::string_view write_data_;
  SizeCallback write_cbk_;
  int write_count_ = 0;
};

}
}
}
