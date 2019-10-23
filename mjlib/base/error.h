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

#include <boost/system/error_code.hpp>

namespace mjlib {
namespace base {

enum class error {
  kJsonParse = 1,
};

boost::system::error_code make_error_code(error);

}
}

namespace boost {
namespace system {

template <>
struct is_error_code_enum<mjlib::base::error> : std::true_type {};

}
}
