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

#include <cstdint>

#include "mjlib/base/noncopyable.h"
#include "mjlib/base/stream.h"

namespace mjlib {
namespace micro {

class FlashInterface : base::NonCopyable {
 public:
  FlashInterface() {}
  virtual ~FlashInterface() {}

  struct Info {
    char* start = nullptr;
    char* end = nullptr;
  };

  /// Get information necessary to read or write the flash.  Reading
  /// may be accomplished by direct memory access.  Writing must use
  /// the ProgramByte method below following an Erase.
  virtual Info GetInfo() = 0;

  /// Erase the entirety of the managed flash section.
  virtual void Erase() = 0;

  virtual void Unlock() = 0;
  virtual void Lock() = 0;

  /// Write a single byte into flash.
  virtual void ProgramByte(char* ptr, uint8_t value) = 0;
};

class FlashWriteStream : public base::WriteStream {
 public:
  FlashWriteStream(FlashInterface& flash, char* start)
      : flash_(flash), position_(start) {}

  void write(const std::string_view& data) override {
    for (uint8_t c : data) {
      flash_.ProgramByte(position_, c);
      position_++;
    }
  }

  void skip(size_t amount) {
    position_ += amount;
  }

  char* position() const { return position_; }

 private:
  FlashInterface& flash_;
  char* position_;
};

}
}
