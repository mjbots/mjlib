// Copyright 2015-2020 Josh Pieper, jjp@pobox.com.
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

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/noncopyable.hpp>

#include <cstdint>
#include <string_view>

#include "mjlib/base/thread_writer.h"
#include "mjlib/telemetry/format.h"

namespace mjlib {
namespace telemetry {

/// Write log files with a format as described in README.md
class FileWriter : boost::noncopyable {
 public:
  using Buffer = base::ThreadWriter::Buffer;

  struct Options {
    /// Write previous offsets for all data records.
    bool write_previous_offsets = true;

    /// Use compression for all data records by default.
    bool default_compression = true;

    int compression_level = 3;

    /// Enable checksums for all data blocks by default.
    bool default_checksum_data = true;

    /// Write a trailing index block.
    bool index_block = true;

    /// Emit seek blocks at this interval.  Note, for this to have an
    /// effect, timestamps must be provided either through the API or
    /// from the system.  A zero value disables seek blocks.
    double seek_block_period_s = 1.0;

    /// If true, then writes may block.
    bool blocking = true;

    /// If timestamps are unspecified, use system timestamps.
    bool timestamps_system = true;

    Options() {}
  };

  FileWriter(const Options& options = {});
  FileWriter(std::string_view filename, const Options& options = {});
  ~FileWriter();

  /// Open the given file for writing.  It will write any queued
  /// schema blocks.  It may be called multiple times.
  void Open(std::string_view filename);

  /// Identical semantics to Open(std::string), but takes a file
  /// descriptor instead.
  void Open(int fd);

  /// Return true if any file is open for writing.
  bool IsOpen() const;

  /// Close the log, this is implicit at destruction time.
  void Close();

  /// Try to write all data to the operating system.  Note, this does
  /// not necessarily mean the data has been written to disk or a
  /// backing store.
  void Flush();

  using Identifier = uint64_t;

  /// Allocate a unique identifier for the given name.
  Identifier AllocateIdentifier(std::string_view record_name);

  /// @return fale if the identifier could not be reserved.
  bool ReserveIdentifier(std::string_view name, Identifier id);

  /// Write a schema block to the log file.
  void WriteSchema(Identifier, std::string_view schema);


  struct Override {
    Override() {}
    Override(bool require_in, bool disable_in)
        : require(require_in), disable(disable_in) {}
    static Override required() { return {true, false}; }
    static Override disabled() { return {false, true}; }

    bool require = false;
    bool disable = false;

    bool evaluate(bool default_value) const {
      if (require) { return true; }
      if (disable) { return false; }
      return default_value;
    }
  };

  struct WriteFlags {
    // Potentially override the default settings.
    Override compression;
    Override checksum;
  };

  /// Write a data block to the log file.
  ///
  /// If timestamp is not default constructed, it must monotonically
  /// increase w.r.t. all calls to Write* functions.  If default
  /// constructed, the time is obtained from the system.
  void WriteData(boost::posix_time::ptime timestamp,
                 Identifier,
                 std::string_view serialized_data,
                 const WriteFlags& = {});

  /// This raw API should only be used if you know what you are doing.
  /// It directly emits a block to the file store, and does not ensure
  /// that the block is properly formatted.
  void WriteBlock(Format::BlockType block_type,
                  std::string_view data);

  /// The following APIs are mimics which can be used to implement
  /// high-performance writing where no additional copies are
  /// necessary.

  /// Get a buffer which can be used to send data.
  Buffer GetBuffer();

  // These variants of Write take a buffer, (which must have been
  // obtained by "GetBuffer" above), and returns ownership of the
  // buffer to the FileWriter class.

  void WriteData(boost::posix_time::ptime timestamp,
                 Identifier,
                 Buffer buffer,
                 const WriteFlags& = {});
  void WriteBlock(Format::BlockType block_type,
                  Buffer buffer);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}
}
