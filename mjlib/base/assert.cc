// Copyright 2018 Josh Pieper, jjp@pobox.com.
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

#include <cstdio>
#include <cstdlib>

namespace mjlib {
namespace base {

void __attribute__((weak)) assertion_failed(const char* expression, const char* filename, int line) {
  ::fprintf(stderr, "\n");
  ::fprintf(stderr, "Assertion Failed: %s:%d %s\n", filename, line, expression);
  ::fflush(stderr);
  ::abort();
}

}
}
