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

#include <boost/filesystem.hpp>

namespace mjlib {
namespace base {

class TemporaryFile {
 public:
  ~TemporaryFile() {
    boost::filesystem::remove(path_);
  }

  std::string native() const { return path_.native(); }
  boost::filesystem::path path() const { return path_; }

 private:
  boost::filesystem::path path_ =
      boost::filesystem::temp_directory_path() /
      boost::filesystem::unique_path();
};

}
}
