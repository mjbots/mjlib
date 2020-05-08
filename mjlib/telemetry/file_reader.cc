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

#include "mjlib/telemetry/file_reader.h"

#include <cstdlib>
#include <cstring>
#include <deque>
#include <set>

#include <boost/crc.hpp>

#include <fmt/format.h>

#include <snappy.h>

#include "mjlib/base/crc_stream.h"
#include "mjlib/base/file_stream.h"
#include "mjlib/base/system_error.h"
#include "mjlib/telemetry/error.h"

namespace mjlib {
namespace telemetry {

namespace {
class FilePtr {
 public:
  FilePtr(std::string_view name) {
    file_ = ::fopen(name.data(), "rb");
    mjlib::base::system_error::throw_if(
        file_ == nullptr,
        fmt::format("When opening: '{}'", name));
    base::system_error::throw_if(::fseek(file_, 0, SEEK_END) < 0);
    size_ = Tell();
    base::system_error::throw_if(::fseek(file_, 0, SEEK_SET) < 0);
  }

  ~FilePtr() {
    ::fclose(file_);
  }

  FILE* file() {
    return file_;
  }

  void Seek(int64_t index) {
    base::system_error::throw_if(::fseek(file_, index, SEEK_SET) < 0);
  }

  int64_t Tell() {
    const auto result = ::ftell(file_);
    base::system_error::throw_if(result < 0);
    return result;
  }

  int64_t size() const { return size_; }

 private:
  FILE* file_ = nullptr;
  int64_t size_ = 0;
};

/// Guarantee that an exact amount is read (or ignored) from an
/// underlying stream.
class BlockStream : public base::ReadStream {
 public:
  BlockStream(base::ReadStream& base, std::streamsize size)
      : base_(base), size_(size) {}

  ~BlockStream() override {
    if (size_ > 0) {
      base_.ignore(size_);
    }
  }

  void ignore(std::streamsize size) override {
    MJ_ASSERT(size <= size_);
    size_ -= size;
    base_.ignore(size);
  }

  void read(const base::string_span& data) override {
    MJ_ASSERT(data.size() <= size_);
    size_ -= data.size();
    base_.read(data);
  }

  std::streamsize gcount() const override {
    return base_.gcount();
  }

  std::streamsize remaining() const {
    return size_;
  }

  void shrink(std::streamsize size) { size_ -= size; }

 private:
  base::ReadStream& base_;
  std::streamsize size_;
};
}  // namespace

class Filter {
 public:
  virtual ~Filter() {}
  virtual bool check(FileReader::Identifier) = 0;
  virtual void new_schema(FileReader::Identifier, const std::string&) = 0;
};

struct FileReader::ItemRangeContext : public Filter {
  ~ItemRangeContext() override {}

  FileReader::Impl* impl = nullptr;
  std::set<std::string> unknown_names;
  std::set<FileReader::Identifier> ids;
  FileReader::ItemsOptions options;

  bool check(Identifier identifier) override {
    if (options.records.empty()) { return true; }
    if (ids.count(identifier)) { return true; }
    return false;
  }

  void new_schema(Identifier identifier, const std::string& name) override {
    if (unknown_names.count(name)) {
      unknown_names.erase(name);
      ids.insert(identifier);
    }
  }
};

class FileReader::Impl {
 public:
  Impl(std::string_view filename, const Options& options)
      : options_(options),
        fptr_(filename) {
    char header[8] = {};
    file_.read(header);
    if (std::memcmp(header, "TLOG0003", 8) != 0) {
      throw base::system_error(errc::kInvalidHeader);
    }
    telemetry::ReadStream stream{file_};
    const auto header_flags = stream.ReadVaruint();
    if (header_flags != 0) {
      // There are no known header flags yet.
      throw base::system_error(errc::kInvalidHeaderFlags);
    }

    start_ = fptr_.Tell();

    MaybeProcessIndex();
  }

  ~Impl() {
  }

  ItemRange items(const ItemsOptions& options) {
    auto context = std::make_shared<ItemRangeContext>();
    context->impl = this;
    context->options = options;
    for (const auto& name : options.records) {
      const auto it = name_to_record_.find(name);
      if (it != name_to_record_.end()) {
        context->ids.insert(it->second->identifier);
      } else {
        context->unknown_names.insert(name);
      }
    }

    return ItemRange(context);
  }

  struct Header {
    Format::BlockType type = {};
    uint64_t size = {};
  };

  std::optional<Header> ReadHeader(base::ReadStream& base_stream,
                                   bool throw_on_error=true) {
    telemetry::ReadStream stream{base_stream};
    const auto maybe_type = stream.ReadVaruint();
    if (!maybe_type) { return {}; }
    const auto type = *maybe_type;
    if (type > static_cast<uint64_t>(Format::BlockType::kNumTypes) ||
        type == 0) {
      if (throw_on_error) {
        throw base::system_error(errc::kInvalidBlockType);
      } else {
        return {};
      }
    }

    const auto size = stream.ReadVaruint().value();
    return Header{static_cast<Format::BlockType>(type), size};
  }

  const Record* ProcessSchema(BlockStream& block_stream, Filter* filter) {
    telemetry::ReadStream stream{block_stream};

    const auto identifier = stream.ReadVaruint().value();
    {
      auto it = id_to_record_.find(identifier);
      if (it != id_to_record_.end()) { return it->second; }
    }

    records_.push_back({});
    auto& record = records_.back();

    record.identifier = identifier;
    record.flags = stream.ReadVaruint().value();

    auto flags = record.flags;
    if (flags) {
      throw base::system_error(errc::kUnknownBlockSchemaFlag);
    }

    record.name = stream.ReadString().value();
    record.raw_schema.resize(block_stream.remaining());
    block_stream.read(record.raw_schema);

    record.schema = std::make_unique<BinarySchemaParser>(
        record.raw_schema, record.name);

    id_to_record_[identifier] = &record;
    name_to_record_[record.name] = &record;

    if (filter) {
      filter->new_schema(record.identifier, record.name);
    }
    return &record;
  }

  std::pair<Index, Index> ReadUntil(Index start, Filter* filter) {
    fptr_.Seek(start);

    while (true) {
      start = fptr_.Tell();

      const auto maybe_header = ReadHeader(file_);
      if (!maybe_header) {
        // EOF
        return std::make_pair(-1, -1);
      }
      const auto& header = *maybe_header;

      BlockStream block_stream{file_, static_cast<std::streamsize>(header.size)};
      telemetry::ReadStream stream{block_stream};

      switch (header.type) {
        case Format::BlockType::kData: {
          if (start > final_item_) { final_item_ = start; }
          const auto identifier = stream.ReadVaruint().value();
          if (!filter->check(identifier)) { break; }

          // This is what we want!
          const auto next = fptr_.Tell() + block_stream.remaining();
          return std::make_pair(start, next);
        }
        case Format::BlockType::kSchema: {
          ProcessSchema(block_stream, filter);
          break;
        }
        case Format::BlockType::kIndex:
        case Format::BlockType::kCompressionDictionary:
        case Format::BlockType::kSeekMarker: {
          break;
        }
      }
    }
  }

  Item Read(Index index) {
    fptr_.Seek(index);

    base::CrcReadStream<boost::crc_32_type> crc_stream{file_};

    const auto maybe_header = ReadHeader(crc_stream);
    MJ_ASSERT(!!maybe_header);
    const auto& header = *maybe_header;

    MJ_ASSERT(header.type == Format::BlockType::kData);
    BlockStream block_stream{
      crc_stream, static_cast<std::streamsize>(header.size)};
    telemetry::ReadStream stream{block_stream};

    Item result;
    result.index = index;
    const auto identifier = stream.ReadVaruint().value();
    result.flags = stream.ReadVaruint().value();

    auto flags = result.flags;
    auto check_flags = [&](auto flag) {
      const auto u64_flag = static_cast<uint64_t>(flag);
      if (flags & u64_flag) {
        flags &= ~u64_flag;
        return true;
      }
      return false;
    };

    if (check_flags(Format::BlockDataFlags::kPreviousOffset)) {
      stream.ReadVaruint(); // discard
    }
    if (check_flags(Format::BlockDataFlags::kTimestamp)) {
      result.timestamp = stream.ReadTimestamp().value();
    }

    std::optional<uint32_t> checksum;
    if (check_flags(Format::BlockDataFlags::kChecksum)) {
      // We need to feed the CRC all 0s.
      const uint32_t all_zeros = 0;
      crc_stream.crc().process_bytes(&all_zeros, sizeof(all_zeros));

      // And then we need to read the CRC on the sly.
      telemetry::ReadStream nocrc_stream{file_};
      checksum = nocrc_stream.Read<uint32_t>().value();

      // And then fudge our block stream.
      block_stream.shrink(sizeof(all_zeros));
    }

    const bool snappy =
        check_flags(Format::BlockDataFlags::kSnappy);

    if (flags != 0) {
      throw base::system_error(errc::kUnknownBlockDataFlag);
    }

    result.data.resize(block_stream.remaining());
    block_stream.read(result.data);

    if (snappy) {
      size_t decompressed_size = 0;
      {
        const bool success = snappy::GetUncompressedLength(
            result.data.data(), result.data.size(),
            &decompressed_size);
        if (!success) {
          throw base::system_error(errc::kDecompressionError);
        }
      }
      std::string decompressed;
      decompressed.resize(decompressed_size);
      {
        const bool success = snappy::RawUncompress(
            result.data.data(), result.data.size(),
            &decompressed[0]);
        if (!success) {
          throw base::system_error(errc::kDecompressionError);
        }
      }
      std::swap(decompressed, result.data);
    }

    if (checksum && options_.verify_checksums) {
      if (*checksum != crc_stream.checksum()) {
        throw base::system_error(
            {errc::kDataChecksumMismatch,
                  fmt::format("Expected checksum 0x{:08x} got 0x{:08x}",
                              crc_stream.checksum(), *checksum)});
      }
    }

    result.record = id_to_record_.at(identifier);

    return result;
  }

  const Record* record(std::string_view name_view) {
    std::string name{name_view};
    if (name_to_record_.count(name)) {
      return name_to_record_.at(name);
    }
    if (all_records_found_) { return nullptr; }

    // We haven't read everything, just do a full scan for now to
    // ensure we've processed everything.
    FullScan();
    if (name_to_record_.count(name)) {
      return name_to_record_.at(name);
    }
    return nullptr;
  }

  std::vector<const Record*> records() {
    if (!all_records_found_) { FullScan(); }
    std::vector<const Record*> result;
    for (const auto& record : records_) {
      result.push_back(&record);
    }
    return result;
  }

  Index final_item() {
    if (!all_records_found_) { FullScan(); }
    return final_item_;
  }

  struct SeekMarkerResult {
    boost::posix_time::ptime timestamp;
    Index index;
    SeekResult seek_result;
  };

  std::optional<SeekMarkerResult> EvaluateSeekMarker(Index index) {
    // @p index points to the last byte of the marker.  See if we can
    // read a valid block out of this.

    const uint8_t header_len = [&]() {
      ReadStream stream{file_};
      stream.Read<uint32_t>();  // crc
      return stream.Read<uint8_t>().value();
    }();
    if (header_len > 10) {
      // The max varuint size is 9 + 1 for the block type.
      return {};
    }
    const auto possible_start = index - 7 - header_len;
    if (possible_start < start_) {
      // It can't be before the beginning of the file.
      return {};
    }

    base::CrcReadStream<boost::crc_32_type> crc_stream{file_};

    fptr_.Seek(possible_start);
    const auto maybe_header = ReadHeader(crc_stream, false);
    if (!maybe_header) { return {}; }
    const auto header = *maybe_header;
    if (header.type != Format::BlockType::kSeekMarker) {
      return {};
    }

    SeekMarkerResult result;
    result.index = possible_start;
    BlockStream block_stream{crc_stream, static_cast<std::streamsize>(header.size)};
    ReadStream stream{block_stream};

    stream.Read<uint64_t>().value();  // marker
    const auto crc = [&]() {
      // We want to tell the crc stream that we had all zeros here.
      const uint32_t all_zeros = 0;
      crc_stream.crc().process_bytes(&all_zeros, sizeof(all_zeros));

      // Tell our block stream we skipped those bytes.
      block_stream.shrink(sizeof(all_zeros));

      // Now we want to read the actual value straight from the file.
      ReadStream nocrc_stream{file_};
      return nocrc_stream.Read<uint32_t>().value();
    }();
    stream.Read<uint8_t>().value();  // header_len
    const auto flags = stream.ReadVaruint().value();
    if (flags) {
      throw base::system_error(errc::kUnknownSeekMarkerFlag);
    }
    result.timestamp = stream.ReadTimestamp().value();
    const auto nelements = stream.ReadVaruint().value();
    for (uint64_t i = 0; i < nelements; i++) {
      const auto identifier = stream.ReadVaruint().value();
      const auto previous_offset = stream.ReadVaruint().value();
      result.seek_result.insert(
          std::make_pair(
              id_to_record_.at(identifier), possible_start - previous_offset));
    }

    if (block_stream.remaining() != 0) {
      return {};
    }

    if (crc != crc_stream.checksum()) {
      // We'll count a checksum mismatch as just meaning we got a
      // false positive.
      return {};
    }

    return result;
  }

  std::optional<SeekMarkerResult> FindSeekMarker(Index index, Index end) {
    fptr_.Seek(index);
    const auto stop_point = std::min(fptr_.size(), end);

    // Do the dumb thing for now, our search pattern is only 8 bytes
    // long after all.
    constexpr uint8_t to_match[] = {
      0x64, 0x75, 0x86, 0x97, 0xa8, 0xb9, 0xca, 0xfd,
    };
    int matched = 0;
    while (true) {
      uint8_t c = {};
      file_.read(base::string_span(reinterpret_cast<char*>(&c), 1));
      if (c == to_match[matched]) {
        matched++;
        if (matched == 8) {
          auto maybe_result = EvaluateSeekMarker(index);
          if (!!maybe_result) {
            return std::move(*maybe_result);
          }
          // Guess not, just keep looking.
          fptr_.Seek(index);
          matched = 0;
        }
      } else {
        matched = 0;
      }
      index++;
      if (index >= stop_point) {
        // We got to the end and haven't found anything.
        return {};
      }
    }
  }

  SeekResult Seek(const boost::posix_time::ptime timestamp) {
    // We need to know about all schemas before we can do this.
    if (!all_records_found_) { FullScan(); }

    // We look exclusively for SeekMarkers, as they have a known
    // signature and guaranteed checksum.  Our strategy is to find two
    // SeekMarkers that bound the given timestamp, then walk from the
    // first to the second updating the results as we go.
    int64_t low = start_;
    int64_t high = final_item_;

    SeekResult result;

    constexpr int64_t kMinSpacing = 1 << 16;
    while ((high - low) > kMinSpacing) {
      const int64_t mid_search_point = low + (high - low) / 2;
      const auto seek = FindSeekMarker(mid_search_point, high);
      if (!seek) {
        // There are no seek points in our second half.  Just linear
        // search from the current low point.
        break;
      }
      if (seek->timestamp <= timestamp) {
        low = seek->index;
        result = seek->seek_result;
      } else {
        high = seek->index;
      }
    }

    ItemsOptions items_options;
    items_options.start = low;
    items_options.end = high;
    for (const auto& item : items(items_options)) {
      if (item.timestamp.is_not_a_date_time()) { break; }
      if (item.timestamp > timestamp) { break; }
      auto& current_last = result[item.record];
      if (item.index > current_last) { current_last = item.index; }
    }

    return result;
  }

  void FullScan() {
    class NoFilter : public Filter {
     public:
      bool check(Identifier) override { return false; }
      void new_schema(Identifier, const std::string&) override {}
    };

    NoFilter no_filter;
    ReadUntil(start_, &no_filter);
    all_records_found_ = true;
  }

  void MaybeProcessIndex() {
    // Seek to 8 bytes from the end.
    fptr_.Seek(fptr_.size() - 8);
    char trailer[8] = {};
    file_.read(trailer);

    if (std::memcmp(trailer, "TLOGIDEX", 8) != 0) {
      // Nope, definitely not an index.
      return;
    }

    // We have something that looks plausibly like an index.  Lets see
    // if it validates as an entire block.
    fptr_.Seek(fptr_.size() - 12);
    telemetry::ReadStream stream{file_};
    const uint32_t trailer_size = stream.Read<uint32_t>().value();
    if (trailer_size >= (fptr_.size() - start_)) {
      // This purported record would be bigger than the entire log.
      return;
    }

    fptr_.Seek(fptr_.size() - trailer_size);
    const auto maybe_header = ReadHeader(file_);
    if (!maybe_header) {
      // Nope.  Some other corruption.
      return;
    }
    const auto header = *maybe_header;
    if (header.type != Format::BlockType::kIndex) {
      // Hmmmph.  Wrong type.
      return;
    }

    // From here on out we'll assume the index was supposed to be
    // here, and thus we'll let other parse errors trickle up as
    // exceptions rather than silently ignoring the index block.
    const auto flags = stream.ReadVaruint().value();
    if (flags != 0) {
      throw base::system_error(errc::kUnknownIndexFlag);
    }

    struct LocalRecord {
      uint64_t identifier = {};
      int64_t schema_location = {};
      int64_t final_record = {};
    };

    std::vector<LocalRecord> local_records;

    const auto nelements = stream.ReadVaruint().value();
    for (uint64_t i = 0; i < nelements; i++) {
      LocalRecord record;
      record.identifier = stream.ReadVaruint().value();
      record.schema_location = static_cast<int64_t>(stream.Read<uint64_t>().value());
      record.final_record = static_cast<int64_t>(stream.Read<uint64_t>().value());
      local_records.push_back(record);
    }

    // Now go and find all the schemas so that we can fill in our
    // records structures.
    MJ_ASSERT(records_.empty());
    final_item_ = 0;
    for (const auto& local_record : local_records) {
      fptr_.Seek(local_record.schema_location);
      const auto header = ReadHeader(file_).value();
      BlockStream block_stream{file_, static_cast<std::streamsize>(header.size)};
      const auto* record = ProcessSchema(block_stream, nullptr);
      MJ_ASSERT(record->identifier == local_record.identifier);
      if (local_record.final_record > final_item_) {
        final_item_ = local_record.final_record;
      }
    }

    has_index_ = true;
    all_records_found_ = true;
  }

  const Options options_;
  FilePtr fptr_;
  base::FileStream file_{fptr_.file()};

  std::deque<Record> records_;
  std::map<Identifier, const Record*> id_to_record_;
  std::map<std::string, const Record*> name_to_record_;

  Index final_item_ = -1;
  bool has_index_ = false;
  bool all_records_found_ = false;
  int64_t start_ = 0;
};

FileReader::FileReader(std::string_view filename, const Options& options)
    : impl_(std::make_unique<Impl>(filename, options)) {}

FileReader::~FileReader() {}

const FileReader::Record* FileReader::record(std::string_view name) {
  return impl_->record(name);
}

std::vector<const FileReader::Record*> FileReader::records() {
  return impl_->records();
}

bool FileReader::has_index() const {
  return impl_->has_index_;
}

FileReader::Index FileReader::final_item() {
  return impl_->final_item();
}

FileReader::SeekResult FileReader::Seek(boost::posix_time::ptime timestamp) {
  return impl_->Seek(timestamp);
}

FileReader::Item FileReader::ItemIterator::operator*() {
  return context_->impl->Read(index_);
}

FileReader::ItemIterator& FileReader::ItemIterator::operator++() {
  // This first call gives us where we started at.
  auto [first, after] = context_->impl->ReadUntil(index_, context_.get());
  auto [advanced, _] = context_->impl->ReadUntil(after, context_.get());
  index_ = advanced;
  return *this;
}

bool FileReader::ItemIterator::operator!=(const ItemIterator& rhs) const {
  return index_ != rhs.index_;
}

FileReader::ItemIterator FileReader::ItemRange::begin() {
  // For now, always start at the very beginning.
  auto [first, next] = context_->impl->ReadUntil(
      context_->options.start < 0 ? context_->impl->start_ :
      context_->options.start,
      context_.get());
  return ItemIterator(context_, first);
}

FileReader::ItemIterator FileReader::ItemRange::end() {
  return ItemIterator(context_, -1);
}

FileReader::ItemRange FileReader::items(const ItemsOptions& options) {
  return impl_->items(options);
}

}
}
