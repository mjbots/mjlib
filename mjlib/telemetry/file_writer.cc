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

#include "mjlib/telemetry/file_writer.h"

#include <cstdio>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include <boost/assert.hpp>
#include <boost/crc.hpp>

#include <fmt/format.h>

#include <snappy.h>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/thread_writer.h"

namespace mjlib {
namespace telemetry {

namespace {
template <typename T>
uint64_t u64(T value) {
  return static_cast<uint64_t>(value);
}

const size_t kBufferStartPadding = 32;

using FilePosition = int64_t;
using Identifier = FileWriter::Identifier;
using base::ThreadWriter;

struct SchemaRecord {
  std::string name;
  Identifier identifier = 0;
  uint64_t block_schema_flags = 0;
  std::string schema;
  FilePosition schema_position;
  FilePosition last_position = -1;

  SchemaRecord(std::string_view name,
               Identifier identifier,
               uint64_t block_schema_flags,
               std::string_view schema,
               FilePosition schema_position)
      : name(name),
        identifier(identifier),
        block_schema_flags(block_schema_flags),
        schema(schema),
        schema_position(schema_position) {}
  SchemaRecord() {}
};
}

class FileWriter::Impl : public ThreadWriter::Reclaimer {
 public:
  Impl(const Options& options)
      : options_(options) {}

  virtual ~Impl() {
    Close();
  }

  ThreadWriter::Options GetWriterOptions() {
    ThreadWriter::Options options;
    options.blocking_mode = (
        options_.blocking ? ThreadWriter::kBlocking :
        ThreadWriter::kAsynchronous);
    options.reclaimer = this;
    return options;
  }

  void Open(std::string_view filename) {
    BOOST_ASSERT(!writer_);
    writer_ = std::make_unique<ThreadWriter>(filename, GetWriterOptions());

    PostOpen();
  }

  void Open(int fd) {
    BOOST_ASSERT(!writer_);
    writer_ = std::make_unique<ThreadWriter>(fd, GetWriterOptions());
    PostOpen();
  }

  void Close() {
    if (!writer_) { return; }

    if (options_.index_block) { WriteIndex(); }
    writer_.reset();
    last_seek_block_ = {};
  }

  void Flush() {
    if (!writer_) { return; }

    writer_->Flush();
  }

  Identifier AllocateIdentifier(std::string_view record_name) {
    auto it = identifier_map_.find(std::string(record_name));
    if (it != identifier_map_.end()) { return it->second; }

    // Find one that isn't used yet.
    while (reverse_identifier_map_.count(next_id_)) {
      next_id_++;
    }

    Identifier result = next_id_;
    next_id_++;

    identifier_map_[std::string(record_name)] = result;
    reverse_identifier_map_[result] = std::string(record_name);

    return result;
  }

  bool ReserveIdentifier(std::string_view record_name, Identifier identifier) {
    {
      const auto it = identifier_map_.find(std::string(record_name));
      if (it != identifier_map_.end()) {
        if (it->second == identifier) { return true; }

        // We've already registered this with a different ID.  That is
        // an error.
        mjlib::base::Fail(
            fmt::format(
                "record name '{}' registered with different ids", record_name));
      }
    }

    {
      const auto it = reverse_identifier_map_.find(identifier);
      if (it != reverse_identifier_map_.end()) {
        // This identifier is already in use.
        return false;
      }
    }

    identifier_map_[std::string(record_name)] = identifier;
    reverse_identifier_map_[identifier] = std::string(record_name);

    return true;
  }

  void Write(Buffer buffer) {
    if (!writer_) { return; }

    writer_->Write(std::move(buffer));
  }

  void WriteData(boost::posix_time::ptime timestamp,
                 Identifier identifier,
                 std::string_view serialized_data,
                 const WriteFlags& write_flags) {
    if (!writer_) { return; }

    auto buffer = GetBuffer();

    // If you're using this API, we'll assume you don't care about
    // performance or the number of copies too much.
    buffer->write(serialized_data);

    WriteData(timestamp, identifier, std::move(buffer), write_flags);
  }

  void WriteBlock(Format::BlockType block_type,
                  std::string_view data) {
    if (!writer_) { return; }

    auto buffer = GetBuffer();

    WriteStream stream(*buffer);
    stream.WriteVaruint(u64(block_type));
    stream.WriteVaruint(u64(data.size()));
    stream.RawWrite(data);

    Write(std::move(buffer));
  }

  void PostOpen() {
    // Write the header.
    {
      auto buffer = GetBuffer();
      WriteStream stream(*buffer);
      buffer->write({"TLOG0003", 8});
      stream.WriteVaruint(0);

      writer_->Write(std::move(buffer));
    }

    for (const auto& pair: schema_) {
      WriteSchema(pair.second.identifier,
                  // Our schemas will be changed from under us, so we
                  // need a temporary copy of this string.
                  std::string(pair.second.schema));
    }
  }

  FilePosition GetPreviousOffset(Identifier identifier) const {
    if (!writer_) { return 0; }

    const auto it = schema_.find(identifier);
    if (it == schema_.end() || it->second.last_position < 0) { return 0; }

    const auto position = writer_->position();
    return position - it->second.last_position;
  }

  FilePosition position() const {
    if (!writer_) { return 0; }
    return writer_->position();
  }

  void Reclaim(Buffer buffer) override {
    std::lock_guard<std::mutex> guard(buffers_mutex_);
    buffers_.push_back(std::move(buffer));
  }

  void WriteSeekBlock(boost::posix_time::ptime timestamp) {
    auto buffer = GetBuffer();
    WriteStream stream(*buffer);

    const int orig_start = buffer->start();

    stream.Write(static_cast<uint64_t>(0xfdcab9a897867564));
    const int crc_pos = orig_start + 8;

    stream.Write(static_cast<uint32_t>(0));  // placeholder crc
    stream.Write(static_cast<uint8_t>(0));  // placeholder header size

    stream.WriteVaruint(0);  // flags
    stream.Write(timestamp);
    const uint64_t num_elements = [&]() {
      uint64_t count = 0;
      for (const auto& pair : schema_) {
        if (pair.second.last_position >= 0) { count++; }
      }
      return count;
    }();
    stream.WriteVaruint(num_elements);
    for (const auto& pair : schema_) {
      if (pair.second.last_position < 0) { continue; }
      stream.WriteVaruint(pair.first);
      stream.WriteVaruint(writer_->position() - pair.second.last_position);
    }

    const auto body_size = buffer->size();
    const auto header_size = 1 + Format::GetVaruintSize(body_size);
    buffer->set_start(buffer->start() - header_size);
    {
      base::BufferWriteStream buffer_stream{
        base::string_span(buffer->data()->data() + buffer->start(),
                          header_size)};
      WriteStream header_stream{buffer_stream};
      header_stream.WriteVaruint(u64(Format::BlockType::kSeekMarker));
      header_stream.WriteVaruint(body_size);
    }
    *(buffer->data()->data() + crc_pos + 4) = header_size;

    boost::crc_32_type crc;
    crc.process_bytes(buffer->data()->data() + buffer->start(),
                      header_size + body_size);

    // Now update the CRC.
    {
      base::BufferWriteStream buffer_stream{
        base::string_span(buffer->data()->data() + crc_pos, 4)};
      WriteStream crc_stream{buffer_stream};
      crc_stream.Write(static_cast<uint32_t>(crc.checksum()));
    }

    Write(std::move(buffer));
  }

  void WriteIndex() {
    auto buffer = GetBuffer();
    WriteStream stream(*buffer);

    const uint64_t flags = 0;
    stream.WriteVaruint(flags);
    uint64_t num_elements = schema_.size();
    stream.WriteVaruint(num_elements);

    for (const auto& pair: schema_) {
      // Write out the BlockIndexRecord for each.
      const auto identifier = pair.first;
      stream.WriteVaruint(identifier);

      const auto& record = pair.second;

      const auto schema_position = record.schema_position;
      stream.Write(u64(schema_position));

      const auto last_data_position = record.last_position;
      stream.Write(u64(last_data_position));
    }

    const uint32_t trailing_size = buffer->size() +
        1 + // block type
        Format::GetVaruintSize(buffer->size() + 4 + 8) +
        4 + // this element itself
        8; // the final 8 byte constant

    stream.Write(trailing_size);
    stream.RawWrite({"TLOGIDEX", 8});

    WriteBlock(Format::BlockType::kIndex, std::move(buffer));
  }

  void WriteSchema(Identifier identifier, std::string_view schema) {
    const auto rit = reverse_identifier_map_.find(identifier);
    if (rit == reverse_identifier_map_.end()) {
      mjlib::base::Fail(fmt::format("unknown id {}", identifier));
    }

    schema_[identifier] = SchemaRecord(
        rit->second, identifier, 0, schema, position());

    base::FastOStringStream ostr_schema;
    WriteStream stream_schema(ostr_schema);
    stream_schema.WriteVaruint(identifier);
    stream_schema.WriteVaruint(0);
    stream_schema.WriteString(rit->second);
    stream_schema.RawWrite(schema);

    auto buffer = GetBuffer();

    WriteStream stream(*buffer);
    stream.WriteVaruint(Format::BlockType::kSchema);
    stream.WriteVaruint(ostr_schema.str().size());
    stream.RawWrite(ostr_schema.str());

    Write(std::move(buffer));
  }

  Buffer GetBuffer() {
    std::lock_guard<std::mutex> guard(buffers_mutex_);

    Buffer result;
    if (buffers_.empty()) {
      result = std::make_unique<ThreadWriter::OStream>();
    } else {
      result = std::move(buffers_.back());
      buffers_.pop_back();
    }
    result->data()->resize(kBufferStartPadding);
    result->set_start(kBufferStartPadding);
    return result;
  }

  void WriteData(boost::posix_time::ptime timestamp,
                 Identifier identifier,
                 Buffer buffer,
                 const WriteFlags& write_flags) {
    if (!writer_) { return; }

    uint64_t block_data_flags = 0;

    uint64_t flag_header_size = 0;

    std::optional<FilePosition> previous_offset;
    if (options_.write_previous_offsets) {
      block_data_flags |= u64(Format::BlockDataFlags::kPreviousOffset);
      previous_offset = GetPreviousOffset(identifier);
      flag_header_size += Format::GetVaruintSize(*previous_offset);
    }

    std::optional<boost::posix_time::ptime> timestamp_to_write;
    if (!timestamp.is_not_a_date_time() || options_.timestamps_system) {
      block_data_flags |= u64(Format::BlockDataFlags::kTimestamp);
      flag_header_size += 8;
      if (!timestamp.is_not_a_date_time()) {
        timestamp_to_write = timestamp;
      } else {
        timestamp_to_write =
            boost::posix_time::microsec_clock::universal_time();
      }
    }

    bool write_checksum = false;
    if (write_flags.checksum.evaluate(options_.default_checksum_data)) {
      block_data_flags |= u64(Format::BlockDataFlags::kChecksum);
      flag_header_size += 4;
      write_checksum = true;
    }

    if (write_flags.compression.evaluate(options_.default_compression)) {
      // We should try to compress this data.
      const auto original_size = buffer->size();

      auto new_buffer = GetBuffer();
      size_t compressed_length = snappy::MaxCompressedLength(original_size);
      new_buffer->data()->reserve(new_buffer->start() + compressed_length);
      snappy::RawCompress(
          buffer->data()->data() + buffer->start(), original_size,
          new_buffer->data()->data() + new_buffer->start(), &compressed_length);
      if (compressed_length < original_size) {
        new_buffer->data()->resize(new_buffer->start() + compressed_length);
        // We got something better.  Add our flag and swap the buffers.
        block_data_flags |= u64(Format::BlockDataFlags::kSnappy);
        std::swap(buffer, new_buffer);
        Reclaim(std::move(new_buffer));
      }
    }

    const auto identifier_size = Format::GetVaruintSize(identifier);
    const auto flag_size = Format::GetVaruintSize(block_data_flags);
    const auto body_size =
        identifier_size + flag_size + flag_header_size + buffer->size();
    const auto header_size =
        identifier_size +
        flag_size +
        flag_header_size +
        Format::GetVaruintSize(body_size) +
        1;  // the block data type

    BOOST_ASSERT(buffer->start() >= header_size);

    base::BufferWriteStream stream(
        {&(*buffer->data())[0] + buffer->start() - header_size,
              static_cast<ssize_t>(header_size)});
    WriteStream writer(stream);
    writer.WriteVaruint(u64(Format::BlockType::kData));
    writer.WriteVaruint(body_size);
    writer.WriteVaruint(identifier);
    writer.WriteVaruint(block_data_flags);

    if (block_data_flags & u64(Format::BlockDataFlags::kPreviousOffset)) {
      writer.WriteVaruint(*previous_offset);
    }

    if (block_data_flags & u64(Format::BlockDataFlags::kTimestamp)) {
      writer.Write(*timestamp_to_write);
    }

    if (write_checksum) {
      auto position = stream.position();
      writer.Write(static_cast<uint32_t>(0));
      stream.reset(position);
      // Now we need to calculate the checksum and put the correct
      // value in.
      boost::crc_32_type crc;
      auto all_data = buffer->view();
      crc.process_bytes(all_data.data() + buffer->start() - header_size,
                        static_cast<std::size_t>(buffer->size() + header_size));
      writer.Write(static_cast<uint32_t>(crc.checksum()));
    }

    buffer->set_start(buffer->start() - header_size);

    schema_[identifier].last_position = writer_->position();

    Write(std::move(buffer));

    if (options_.seek_block_period_s != 0.0) {
      if (last_seek_block_.is_not_a_date_time()) {
        last_seek_block_ = timestamp;
      } else if (!timestamp.is_not_a_date_time() &&
                 (timestamp - last_seek_block_) >= seek_block_period_) {
        WriteSeekBlock(timestamp);
        last_seek_block_ = timestamp;
      }
    }
  }

  void WriteBlock(Format::BlockType block_type,
                  Buffer buffer) {
    if (!writer_) { return; }

    size_t data_size = buffer->size();

    const auto block_size = 1 + Format::GetVaruintSize(buffer->size());

    base::BufferWriteStream stream(
        {&(*buffer->data())[0] + buffer->start() - block_size,
              static_cast<ssize_t>(block_size)});
    BOOST_ASSERT(buffer->start() >= block_size);

    WriteStream writer(stream);
    writer.WriteVaruint(u64(block_type));
    writer.WriteVaruint(u64(data_size));
    buffer->set_start(buffer->start() - block_size);

    Write(std::move(buffer));
  }

  const Options options_;
  const boost::posix_time::time_duration seek_block_period_{
    mjlib::base::ConvertSecondsToDuration(options_.seek_block_period_s)};
  std::unique_ptr<ThreadWriter> writer_;

  std::map<std::string, Identifier> identifier_map_;
  std::map<Identifier, std::string> reverse_identifier_map_;

  Identifier next_id_ = 1;

  std::mutex buffers_mutex_;
  std::vector<Buffer> buffers_;

  std::map<Identifier, SchemaRecord> schema_;
  boost::posix_time::ptime last_seek_block_;
};

FileWriter::FileWriter(const Options& options)
    : impl_(std::make_unique<Impl>(options)) {}

FileWriter::FileWriter(std::string_view filename, const Options& options)
    : impl_(std::make_unique<Impl>(options)) {
  Open(filename);
}

FileWriter::~FileWriter() {}

void FileWriter::Open(std::string_view filename) {
  impl_->Open(filename);
}

void FileWriter::Open(int fd) {
  impl_->Open(fd);
}

bool FileWriter::IsOpen() const {
  return !!impl_->writer_;
}

void FileWriter::Close() {
  impl_->Close();
}

void FileWriter::Flush() {
  impl_->Flush();
}

FileWriter::Identifier FileWriter::AllocateIdentifier(
    std::string_view record_name) {
  return impl_->AllocateIdentifier(record_name);
}

bool FileWriter::ReserveIdentifier(std::string_view record_name,
                                   Identifier identifier) {
  return impl_->ReserveIdentifier(record_name, identifier);
}

void FileWriter::WriteSchema(Identifier identifier, std::string_view schema) {
  impl_->WriteSchema(identifier, schema);
}

void FileWriter::WriteData(boost::posix_time::ptime timestamp,
                           Identifier identifier,
                           std::string_view serialized_data,
                           const WriteFlags& write_flags) {
  impl_->WriteData(timestamp, identifier, serialized_data, write_flags);
}

void FileWriter::WriteBlock(Format::BlockType block_type,
                            std::string_view data) {
  impl_->WriteBlock(block_type, data);
}

FileWriter::Buffer FileWriter::GetBuffer() {
  return impl_->GetBuffer();
}

void FileWriter::WriteData(boost::posix_time::ptime timestamp,
                           Identifier identifier,
                           Buffer buffer,
                           const WriteFlags& write_flags) {
  impl_->WriteData(timestamp, identifier, std::move(buffer), write_flags);
}

void FileWriter::WriteBlock(Format::BlockType block_type,
                            Buffer buffer) {
  impl_->WriteBlock(block_type, std::move(buffer));
}

}
}
