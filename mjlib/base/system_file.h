// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <stdio.h>

namespace mjlib {
namespace base {

/// Manages ownership of a system file descriptor.
class SystemFile {
 public:
  SystemFile() : fd_(nullptr) {}
  SystemFile(FILE* file) : fd_(file) {}

  SystemFile(SystemFile&& rhs) noexcept {
    fd_ = rhs.fd_;
    rhs.fd_ = nullptr;
  }

  SystemFile& operator=(SystemFile&& rhs) noexcept {
    if (this != &rhs) {
      // Release the FILE* this currently owns before adopting the
      // source's. Without this the previous file (and its fd) leaked.
      if (fd_) { ::fclose(fd_); }
      fd_ = rhs.fd_;
      rhs.fd_ = nullptr;
    }
    return *this;
  }

  ~SystemFile() {
    // fclose(NULL) is undefined behavior; on glibc it segfaults.
    // fd_ is null after default construction or after the source of
    // a move.
    if (fd_) { ::fclose(fd_); }
  }

  SystemFile(const SystemFile&) = delete;
  SystemFile& operator=(const SystemFile&) = delete;

  operator FILE*() { return fd_; }
  operator FILE*() const { return fd_; }

 private:
  FILE* fd_ = nullptr;
};

}
}
