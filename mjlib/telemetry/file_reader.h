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

#include <memory>
#include <string>

#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// Read log files with a format as described in README.md
class FileReader {
 private:
  class Impl;
 public:
  struct Options {
    Options() {}
  };

  FileReader(std::string_view filename, const Options& options = {});
  ~FileReader();

  using Identifier = uint64_t;

  /// An opaque token used to refer to positions in the log file.
  using Index = int64_t;

  struct Record {
    /// An arbitrary identifier.  Should typically not be used by
    /// clients.
    Identifier identifier = {};

    std::string name;
    std::string schema;

    /// The flags as set in the log, corresponding to
    /// Format::BlockSchemaFlags
    uint64_t flags = {};
  };

  Record record(std::string_view);
  std::vector<Record> records();

  Index Seek(boost::posix_time::ptime);

  struct ItemsOptions {
    std::vector<std::string> records;
    Index start = -1;
    Index end = -1;

    ItemsOptions() {}
  };

  struct Item {
    Index index = {};

    boost::posix_time::ptime timestamp;
    std::string data;

    // Format::BlockDataFlags
    uint64_t flags = {};
    const Record* record = nullptr;
  };

  struct ItemIterator {
    Item operator*();
    ItemIterator& operator++();

   private:
    Impl* impl_ = nullptr;
    Index next_index_ = -1;
    Identifier identifier_ = {};
  };

  struct ItemRange {
    ItemIterator begin();
    ItemIterator end();

   private:
    Impl* impl_ = nullptr;
    Index first_index_ = -1;
    Identifier identifier_ = {};
  };

  ItemRange items(const ItemsOptions& = {});

 private:
  std::unique_ptr<Impl> impl_;
};

}
}
