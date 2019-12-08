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

#include <string_view>

#include "mjlib/base/string_span.h"

#include "mjlib/micro/async_types.h"

namespace mjlib {
namespace multiplex {

class MicroDatagramServer {
 public:
  virtual ~MicroDatagramServer() {}

  struct Header {
    int source = 0;
    int destination = 0;
    int size = 0;
  };

  virtual void AsyncRead(Header*, const base::string_span&,
                         const micro::SizeCallback&) = 0;

  virtual void AsyncWrite(const Header&, const std::string_view&,
                          const micro::SizeCallback&) = 0;

  struct Properties {
    int max_size = -1;
  };

  virtual Properties properties() const = 0;
};

}
}
