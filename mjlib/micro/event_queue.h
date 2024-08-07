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

#include <deque>

#include "mjlib/micro/event.h"

namespace mjlib {
namespace micro {

class EventQueue {
 public:
  EventPoster MakePoster() {
    return [this](VoidCallback cbk) {
      this->Post(std::move(cbk));
    };
  }

  void Post(VoidCallback cbk) {
    events_.push_back(std::move(cbk));
  }

  void Poll() {
    while (!events_.empty()) {
      auto copy = events_;
      events_ = {};
      for (auto& item : copy) {
        item();
      }
    }
  }

  bool empty() const {
    return events_.empty();
  }

 private:
  std::deque<VoidCallback> events_;
};

}
}
