// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

// Regression check that mjlib/micro/pool_ptr.h is self-contained.
// Other test files include the boost test driver, which transitively
// pulls in <new> and so masks the original bug. This file
// deliberately includes ONLY pool_ptr.h before any other header,
// then instantiates PoolPtr to force the placement-new in the
// constructor template to be parsed and instantiated. The build is
// the test.
#include "mjlib/micro/pool_ptr.h"

namespace {
[[maybe_unused]] int Use() {
  mjlib::micro::SizedPool<64> pool;
  mjlib::micro::PoolPtr<int> p(&pool);
  *p = 7;
  return *p;
}
}  // namespace
