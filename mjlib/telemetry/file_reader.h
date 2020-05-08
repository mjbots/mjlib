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

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mjlib/telemetry/binary_schema_parser.h"
#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// Read log files with a format as described in README.md
class FileReader {
 private:
  class Impl;
 public:
  struct Options {
    bool verify_checksums = true;

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
    std::string raw_schema;
    std::unique_ptr<BinarySchemaParser> schema;

    /// The flags as set in the log, corresponding to
    /// Format::BlockSchemaFlags
    uint64_t flags = {};
  };

  const Record* record(std::string_view);
  std::vector<const Record*> records();

  bool has_index() const;

  Index final_item();

  /// The most recent index for all known records.  If no instance
  /// exists at or prior to the given timestamp, that entry is
  /// absent.
  using SeekResult = std::map<const Record*, Index>;

  /// Find the most recent records that are equal to or before given
  /// timestamp.
  SeekResult Seek(boost::posix_time::ptime timestamp);

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

  struct ItemRangeContext;

  struct ItemIterator {
    ItemIterator(std::shared_ptr<ItemRangeContext> context,
                 Index index)
        : context_(context),
          index_(index) {}

    Item operator*();
    ItemIterator& operator++();
    bool operator!=(const ItemIterator&) const;

   private:
    std::shared_ptr<ItemRangeContext> context_ = nullptr;
    Index index_ = -1;
  };

  struct ItemRange {
    ItemRange(std::shared_ptr<ItemRangeContext> context)
        : context_(context) {}

    ItemIterator begin();
    ItemIterator end();

   private:
    std::shared_ptr<ItemRangeContext> context_;
  };

  ItemRange items(const ItemsOptions& = {});

 private:
  std::unique_ptr<Impl> impl_;
};

}
}
