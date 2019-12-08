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

#include "mjlib/base/visitor.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/pool_ptr.h"

#include "mjlib/multiplex/micro_datagram_server.h"

namespace mjlib {
namespace multiplex {

class MicroStreamDatagram : public MicroDatagramServer {
 public:
  struct Options {
    size_t buffer_size = 256;
  };

  MicroStreamDatagram(micro::Pool*, micro::AsyncStream*, const Options&);
  ~MicroStreamDatagram();

  void AsyncRead(Header*, const base::string_span&,
                 const micro::SizeCallback&) override;
  void AsyncWrite(const Header&, const std::string_view&,
                  const micro::SizeCallback&) override;
  Properties properties() const override;

  // Exposed mostly for debugging and unit testing.
  struct Stats {
    uint32_t checksum_mismatch = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(checksum_mismatch));
    }
  };

  const Stats* stats() const;

 private:
  class Impl;
  micro::PoolPtr<Impl> impl_;
};

}
}
