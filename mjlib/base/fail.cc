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

#include "fail.h"

#ifndef _WIN32
#include <cxxabi.h>
#include <execinfo.h>
#endif  // _WIN32
#include <string.h>

#include <iostream>

#include "mjlib/base/error_code.h"

namespace mjlib {
namespace base {

void AssertNotReached() {
  Fail("assert not reached");
}

namespace {
#ifndef _WIN32
std::string FormatFrame(const std::string& frame) {
  size_t openparen = frame.find_first_of('(');
  if (openparen == std::string::npos) { return frame; }
  size_t closeparen = frame.find_first_of(')');
  if (closeparen == std::string::npos) { return frame; }

  if (closeparen < openparen) { return frame; }

  const std::string file = frame.substr(0, openparen);
  const std::string symbol_offset =
      frame.substr(openparen + 1, closeparen - openparen - 1);
  const std::string rest = frame.substr(closeparen + 1);

  size_t plus = symbol_offset.find_last_of('+');
  if (plus == std::string::npos) { return frame; }

  const std::string symbol = symbol_offset.substr(0, plus);
  const std::string offset = symbol_offset.substr(plus + 1);

  int status = -1;
  char* demangled =
      abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
  if (status != 0) {
    if (demangled) { ::free(demangled); }
    return frame;
  }

  std::string result = file + "(" + demangled + "+" + offset + ")" + rest;
  ::free(demangled);
  return result;
}
#endif  // _WIN32
}

void Fail(const std::string& message) {
#ifndef _WIN32
  const int kMaxFrames = 100;
  void *buffer[kMaxFrames] = {};
  int frames = ::backtrace(buffer, kMaxFrames);

  std::cerr << "Fatal error:\n";
  char **strings = ::backtrace_symbols(buffer, frames);

  for (int i = 0; i < frames; i++) {
    std::cerr << FormatFrame(strings[i]) << "\n";
  }
#endif  // _WIN32

  std::cerr << "\n" << message << "\n\n";

  ::abort();
}

void FailIf(const error_code& ec) {
  if (ec) { Fail(ec.message()); }
}

void FailIfErrno(bool terminate) {
  if (terminate) { Fail(strerror(errno)); }
}

}
}
