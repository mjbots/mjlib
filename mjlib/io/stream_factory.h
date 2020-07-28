// Copyright 2015-2020 Josh Pieper, jjp@pobox.com.
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

#include <map>

#include <boost/asio/any_io_executor.hpp>

#include "mjlib/base/args_visitor.h"
#include "mjlib/io/async_stream.h"

namespace mjlib {
namespace io {

/// Construct an AsyncStream using runtime provided type and
/// parameters.
class StreamFactory : boost::noncopyable {
 public:
  enum class Type {
    kStdio,
    kSerial,
    kTcpClient,
    kTcpServer,
    kPipe,
  };

  static std::map<Type, const char*> TypeMapper();

  struct Options {
    Type type = Type::kTcpServer;

    int stdio_in = 0;
    int stdio_out = 1;

    std::string serial_port;
    int serial_baud = 115200;
    std::string serial_parity = "n";
    int serial_data_bits = 8;
    bool serial_low_latency = true;

    std::string tcp_target;
    int tcp_target_port = 0;

    int tcp_server_port = 0;

    std::string pipe_key;
    int pipe_direction = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVPT(type));
      a->Visit(MJ_NVPT(stdio_in).label("FD"));
      a->Visit(MJ_NVPT(stdio_out).label("FD"));
      a->Visit(MJ_NVPT(serial_port).label("PORT"));
      a->Visit(MJ_NVPT(serial_baud).label("BAUD"));
      a->Visit(MJ_NVPT(serial_parity).label("PARITY").help("n|o|e"));
      a->Visit(MJ_NVP(serial_data_bits));
      a->Visit(MJ_NVP(serial_low_latency));
      a->Visit(MJ_NVP(tcp_target));
      a->Visit(MJ_NVP(tcp_target_port));
      a->Visit(MJ_NVP(tcp_server_port));
      a->Visit(MJ_NVP(pipe_key));
      a->Visit(MJ_NVP(pipe_direction));
    }
  };

  StreamFactory(const boost::asio::any_io_executor&);
  ~StreamFactory();

  /// Create a new stream asynchronously.  The stream will be passed
  /// to @p handler upon completion.
  void AsyncCreate(const Options&, StreamHandler handler);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace io

namespace base {
template <>
struct IsEnum<io::StreamFactory::Type> {
  static constexpr bool value = true;

  static std::map<io::StreamFactory::Type, const char*> map() {
    return io::StreamFactory::TypeMapper();
  }
};

}  // namespace base
}  // namespace mjlib
