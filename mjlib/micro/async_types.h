// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include <cstddef>

#include "mjlib/base/inplace_function.h"

#include "mjlib/micro/error_code.h"

namespace mjlib {
namespace micro {

using VoidCallback = base::inplace_function<void (void)>;
using ErrorCallback = base::inplace_function<void (const error_code&)>;
using SizeCallback = base::inplace_function<void (const error_code&, std::ptrdiff_t)>;

}
}
