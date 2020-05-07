// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include <cstdlib>

#include "mjlib/base/stream.h"
#include "mjlib/base/system_error.h"

namespace mjlib {
namespace base {

class FileStream : public ReadStream {
 public:
  FileStream(FILE* file) : file_(file) {}
  ~FileStream() override {}

  void ignore(std::streamsize size) override {
    mjlib::base::system_error::throw_if(::fseek(file_, size, SEEK_CUR) < 0);
  }

  void read(const string_span& data) override {
    gcount_ = ::fread(data.data(), 1, data.size(), file_);
  }

  std::streamsize gcount() const override {
    return gcount_;
  }

 private:
  FILE* file_ = nullptr;
  std::streamsize gcount_ = 0;
};

}
}
