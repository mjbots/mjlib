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
    mjlib::base::system_error::throw_if(file_ == nullptr);
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

    // TODO: Look for an index.
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

  std::optional<Header> ReadHeader() {
    telemetry::ReadStream stream{file_};
    const auto maybe_type = stream.ReadVaruint();
    if (!maybe_type) { return {}; }
    const auto type = *maybe_type;
    if (type > static_cast<uint64_t>(Format::BlockType::kNumTypes) ||
        type == 0) {
      throw base::system_error(errc::kInvalidBlockType);
    }

    const auto size = stream.ReadVaruint().value();
    return Header{static_cast<Format::BlockType>(type), size};
  }

  void ProcessSchema(BlockStream& block_stream, Filter* filter) {
    telemetry::ReadStream stream{block_stream};

    const auto identifier = stream.ReadVaruint().value();
    if (id_to_record_.count(identifier) != 0) { return; }

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

    filter->new_schema(record.identifier, record.name);
  }

  std::pair<Index, Index> ReadUntil(Index start, Filter* filter) {
    fptr_.Seek(start);

    while (true) {
      start = fptr_.Tell();

      const auto maybe_header = ReadHeader();
      if (!maybe_header) {
        // EOF
        return std::make_pair(-1, -1);
      }
      const auto& header = *maybe_header;

      BlockStream block_stream{file_, static_cast<std::streamsize>(header.size)};
      telemetry::ReadStream stream{block_stream};

      switch (header.type) {
        case Format::BlockType::kData: {
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

    const auto maybe_header = ReadHeader();
    MJ_ASSERT(!!maybe_header);
    const auto& header = *maybe_header;

    MJ_ASSERT(header.type == Format::BlockType::kData);
    BlockStream block_stream{file_, static_cast<std::streamsize>(header.size)};
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

    const bool zstandard =
        check_flags(Format::BlockDataFlags::kZStandard);
    MJ_ASSERT(!zstandard);  // TODO

    if (flags != 0) {
      throw base::system_error(errc::kUnknownBlockDataFlag);
    }

    result.data.resize(block_stream.remaining());
    block_stream.read(result.data);

    result.record = id_to_record_.at(identifier);

    return result;
  }

  const Record* record(std::string_view name_view) {
    std::string name{name_view};
    if (name_to_record_.count(name)) {
      return name_to_record_.at(name);
    }
    if (read_everything_) { return nullptr; }

    // We haven't read everything, just do a full scan for now to
    // ensure we've processed everything.
    FullScan();
    if (name_to_record_.count(name)) {
      return name_to_record_.at(name);
    }
    return nullptr;
  }

  std::vector<const Record*> records() {
    if (!read_everything_) { FullScan(); }
    std::vector<const Record*> result;
    for (const auto& record : records_) {
      result.push_back(&record);
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
    ReadUntil(8, &no_filter);
  }

  const Options options_;
  FilePtr fptr_;
  base::FileStream file_{fptr_.file()};

  std::deque<Record> records_;
  std::map<Identifier, const Record*> id_to_record_;
  std::map<std::string, const Record*> name_to_record_;

  bool read_everything_ = false;
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

FileReader::Index FileReader::Seek(boost::posix_time::ptime) {
  return 8;
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
      context_->options.start < 0 ? 8 : context_->options.start,
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
