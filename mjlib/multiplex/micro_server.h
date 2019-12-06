// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/persistent_config.h"
#include "mjlib/micro/pool_ptr.h"

#include "mjlib/multiplex/format.h"
#include "mjlib/multiplex/micro_datagram_server.h"

namespace mjlib {
namespace multiplex {

/// Implements a multi-node frame based packet protocol on top of an
/// AsyncStream.  This node's ID is stored in a PersistentConfig.
///
/// No dynamic memory is used.
class MicroServer : public Format {
 public:
  /// Applications implementing the server should provide a concrete
  /// implementation of this interface.  Within a single frame, all
  /// calls to Write or Read will take place before returning to the
  /// event loop.  Applications may use this fact to implement atomic
  /// updates as necessary.
  class Server {
   public:
    virtual ~Server() {}

    /// Attempt to store the given value.
    virtual uint32_t Write(Register, const Value&) = 0;

    /// @param type_index is an index into the Value variant
    /// describing what type to return.
    virtual ReadResult Read(Register, size_t type_index) const = 0;
  };

  struct Options {
    int buffer_size = 256;
    int max_tunnel_streams = 1;
    uint8_t default_id = 1;
  };

  MicroServer(micro::Pool*, MicroDatagramServer*, const Options&);
  ~MicroServer();

  /// Allocate a "tunnel", where an AsyncStream is tunneled over the
  /// multiplex connection.
  micro::AsyncStream* MakeTunnel(uint32_t id);

  void Start(Server*);

  // Exposed mostly for debugging and unit testing.
  struct Stats {
    uint32_t wrong_id = 0;
    uint32_t receive_overrun = 0;
    uint32_t unknown_subframe = 0;
    uint32_t missing_subframe = 0;
    uint32_t malformed_subframe = 0;
    uint32_t write_error = 0;
    uint32_t last_write_error = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(wrong_id));
      a->Visit(MJ_NVP(receive_overrun));
      a->Visit(MJ_NVP(unknown_subframe));
      a->Visit(MJ_NVP(missing_subframe));
      a->Visit(MJ_NVP(malformed_subframe));
      a->Visit(MJ_NVP(write_error));
      a->Visit(MJ_NVP(last_write_error));
    }
  };

  const Stats* stats() const;

  struct Config {
    uint8_t id = 1;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(id));
    }
  };

  Config* config();

 private:
  class Impl;
  micro::PoolPtr<Impl> impl_;
};

}
}
