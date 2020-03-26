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

#include <string>

namespace mjlib {
namespace base {

class error_code;

/// Terminate the program.
void AssertNotReached() __attribute__ ((noreturn));

/// Terminate the program and display a backtrace.
void Fail(const std::string& message) __attribute__ ((noreturn));

/// Terminate a program if the given error code is set.
void FailIf(const error_code&);

/// Terminate the program if @p terminate is true, formatting the
/// current errno as part of the message.
void FailIfErrno(bool terminate);

}
}
