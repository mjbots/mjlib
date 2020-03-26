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

#include <string>
#include <vector>

namespace mjlib {
namespace base {

/// This is a helper class to make it easier to construct synthetic
/// argc and argv values from C++ string lists.
///
/// It can be annoying to create values with the proper const-ness, so
/// this does it for you.
struct Args {
  Args(const std::vector<std::string>& items)
      : argc(items.size()), items_(items) {
    for (auto& item : items_) {
      argv_vector_.push_back(&item[0]);
    }
    argv = argv_vector_.data();
  }

  int argc = -1;
  char** argv = nullptr;

  std::vector<std::string> items_;
  std::vector<char*> argv_vector_;
};

}
}
