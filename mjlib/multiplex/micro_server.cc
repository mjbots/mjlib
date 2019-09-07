// Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/micro_server.h"

#include <functional>

#include <boost/crc.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/visitor.h"

#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

namespace {
using BufferReadStream = multiplex::ReadStream<base::BufferReadStream>;
using BufferWriteStream = multiplex::WriteStream<base::BufferWriteStream>;
}

class MicroServer::Impl {
 public:
  class RawStream : public micro::AsyncWriteStream {
   public:
    RawStream(Impl* parent) : parent_(parent) {}

    ~RawStream() override {}

    void AsyncWriteSome(const std::string_view& buffer,
                        const micro::SizeCallback& callback) override {
      MJ_ASSERT(!parent_->write_outstanding_);
      parent_->raw_write_callback_ = callback;
      parent_->stream_->AsyncWriteSome(
          buffer,
          std::bind(&Impl::HandleWriteRaw, parent_,
                    std::placeholders::_1, std::placeholders::_2));
    }

   private:
    Impl* const parent_;
  };

  class TunnelStream : public micro::AsyncStream {
   public:
    void set_parent(Impl* impl) { parent_ = impl; }
    uint32_t id() const { return id_; }
    void set_id(uint32_t id) { id_ = id; }

    void AsyncReadSome(const base::string_span& buffer,
                       const micro::SizeCallback& callback) override {
      MJ_ASSERT(read_buffer_.empty());
      read_buffer_ = buffer;
      read_callback_ = callback;

      // TODO: Return immediately for size 0 reads (may need an event
      // queue).

      DoReadTransfer();
    }

    void AsyncWriteSome(const std::string_view& buffer,
                        const micro::SizeCallback& callback) override {
      MJ_ASSERT(write_buffer_.empty());
      write_buffer_ = buffer;
      write_callback_ = callback;

      // TODO: Return immediately for size 0 writes (may need an event
      // queue).
    }

    void DoReadTransfer() {
      if (read_buffer_.empty() ||
          read_data_size_ == 0) {
        return;
      }

      const auto to_copy = std::min<std::streamsize>(
          read_buffer_.size(), read_data_size_);
      std::memcpy(read_buffer_.data(), read_data_, to_copy);
      std::memmove(read_data_, &read_data_[to_copy], read_data_size_ - to_copy);
      read_data_size_ -= to_copy;

      read_buffer_ = {};

      auto callback = read_callback_;
      read_callback_ = {};

      callback({}, to_copy);
    }

    Impl* parent_ = nullptr;
    uint32_t id_ = 0;

    base::string_span read_buffer_;
    micro::SizeCallback read_callback_;

    char read_data_[128] = {};
    ssize_t read_data_size_ = 0;

    std::string_view write_buffer_;
    micro::SizeCallback write_callback_;
  };

  Impl(micro::Pool* pool,
       micro::AsyncStream* stream,
       const Options& options)
      : options_(options),
        stream_(stream),
        read_buffer_(static_cast<char*>(
                         pool->Allocate(options.buffer_size, 1))),
        write_buffer_(static_cast<char*>(
                          pool->Allocate(options.buffer_size, 1))) {
    config_.id = options.default_id;
    for (auto& tunnel : tunnels_) {
      tunnel.set_parent(this);
    }
  }

  micro::AsyncStream* MakeTunnel(uint32_t id) {
    MJ_ASSERT(id != 0);
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == 0) {
        // This one is unallocated.
        tunnel.set_id(id);
        return &tunnel;
      }
    }

    MJ_ASSERT(false);
    return nullptr;
  }

  void Start(Server* server) {
    server_ = server;
    MaybeStartReadFrame();
  }

  void AsyncReadUnknown(const base::string_span& buffer,
                        const micro::SizeCallback& callback) {
    MJ_ASSERT(unknown_buffer_.empty());
    MJ_ASSERT(!buffer.empty());

    unknown_buffer_ = buffer;
    unknown_callback_ = callback;
  }

  micro::AsyncWriteStream* raw_write_stream() {
    return &raw_write_stream_;
  }

  const Stats* stats() const { return &stats_; }
  Config* config() { return &config_; }

 private:
  void MaybeStartReadFrame() {
    if (read_outstanding_) { return; }

    read_outstanding_ = true;
    stream_->AsyncReadSome(
        base::string_span(&read_buffer_[read_start_],
                          read_buffer_ + options_.buffer_size),
        std::bind(&Impl::HandleReadFrame, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleReadFrame(const micro::error_code& ec, size_t size) {
    read_outstanding_ = false;

    if (ec) {
      // We don't really have a way to log or do anything here.  So
      // lets just bail and start over.
      read_start_ = 0;
      MaybeStartReadFrame();
      return;
    }

    // Advance read_start_ to where our next invalid byte is.
    read_start_ += size;

    for (;;) {
      if (!HandleMaybeFrame()) {
        break;
      }
    }

    MaybeStartReadFrame();
  }

  // Return true if more data might be available in the buffer.
  bool HandleMaybeFrame() {
    // For now, we'll require that we fit a whole valid packet into
    // our buffer at once before acting on it.  So we'll just keep
    // calling StartReadFrame until we get there.

    // Work to start our buffer out with the frame header.

    char* const found = static_cast<char*>(
        std::memchr(read_buffer_, (kHeader & 0xff), read_start_));
    if (found == nullptr) {
      // We don't have anything which could be a header in our
      // buffer.  Just wipe it all out and start over.
      read_start_ = 0;
      return false;
    }

    Consume(found - read_buffer_);

    if (read_start_ < 2) {
      // We need more to even have a header.
      return false;
    }

    if (static_cast<uint8_t>(read_buffer_[1]) != ((kHeader >> 8) & 0xff)) {
      // We had the first byte of a header, but not the second byte.

      // Move out this false start and try again.
      Consume(2);
      return true;
    }

    // We need at least 7 bytes to have a minimal frame.
    if (read_start_ < (kHeaderSize + kCrcSize + kMinVaruintSize)) {
      return false;
    }

    // See if we have enough data to have a valid varuint for size.
    base::BufferReadStream data({&read_buffer_[2],
            static_cast<size_t>(read_start_ - 2)});
    BufferReadStream read_stream{data};

    const auto maybe_source_id = read_stream.Read<uint8_t>();
    const auto maybe_dest_id = read_stream.Read<uint8_t>();
    const auto maybe_size = read_stream.ReadVaruint();

    if (!maybe_size) {
      // We don't have enough for the size yet.
      return false;
    }

    const auto payload_size = *maybe_size;
    if (payload_size > 4096) {
      // We'll claim this is guaranteed to be too big.  Just wipe
      // everything out and start over.
      read_start_ = 0;
      return false;
    }

    if (payload_size > (options_.buffer_size - 7))  {
      // We can't fit this either.
      read_start_ = 0;
      return false;
    }

    // Do we have enough data yet?
    const char* const payload_start = data.position();
    const auto data_we_have =
        &read_buffer_[read_start_] - data.position();
    if (data_we_have < static_cast<ssize_t>(payload_size + 2)) {
      // We need more still.
      return false;
    }

    const char* const crc_location =
        data.position() + payload_size;

    // Woohoo.  We nominally have enough for a whole frame.  Verify
    // the checksum!
    boost::crc_ccitt_type crc;
    crc.process_bytes(read_buffer_, crc_location - read_buffer_);
    const uint16_t expected_crc = crc.checksum();

    data.ignore(payload_size);
    const auto maybe_actual_crc = read_stream.Read<uint16_t>();
    MJ_ASSERT(!!maybe_actual_crc);
    const auto actual_crc = *maybe_actual_crc;

    if (expected_crc != actual_crc) {
      // Whoops, we should log this checksum mismatch somewhere.
      // Assume that we are no longer synchronized and start from the
      // next header possibility.
      stats_.checksum_mismatch++;
      Consume(2);
      return true;
    }

    if ((*maybe_dest_id == kBroadcastId) ||
        (*maybe_dest_id != config_.id)) {
      stats_.wrong_id++;
      const auto total_size = data.position() - read_buffer_;

      if (!unknown_buffer_.empty()) {
        std::memcpy(unknown_buffer_.data(), read_buffer_, total_size);
        auto callback = unknown_callback_;

        unknown_buffer_ = {};
        unknown_callback_ = {};

        callback(micro::error_code(), total_size);
      }

      if (*maybe_dest_id != kBroadcastId) {
        Consume(total_size);
        return true;
      }
    }

    base::BufferWriteStream buffer_write_stream{
      base::string_span(write_buffer_, options_.buffer_size)};
    BufferWriteStream write_stream{buffer_write_stream};
    const bool need_response =
        ((*maybe_source_id) & 0x80) != 0 &&
        !write_outstanding_;

    // Everything checked out.  Now we we can process our subframes.
    ProcessSubframes(
        std::string_view(payload_start, payload_size),
        &buffer_write_stream,
        need_response ? &write_stream : nullptr);
    const auto to_consume = crc_location - read_buffer_ + 2;
    Consume(to_consume);

    if (need_response) {
      WriteResponse(*maybe_source_id & 0x7f, buffer_write_stream.offset());
    }

    return true;
  }

  void Consume(std::streamsize size) {
    if (size == 0) { return; }
    MJ_ASSERT(read_start_ >= size);
    std::memmove(read_buffer_, &read_buffer_[size], read_start_ - size);
    read_start_ -= size;
  }

  void WriteResponse(uint8_t client_id, std::streamsize response_size) {
    MJ_ASSERT(!write_outstanding_);

    // First, figure out how big our size varuint will end up being.
    const auto header_size =
        2 + // kHeader
        1 + // source id
        1 + // dest_id
        GetVaruintSize(response_size);
    std::memmove(&write_buffer_[header_size], &write_buffer_[0], response_size);

    {
      base::BufferWriteStream header_buffer_stream(
          base::string_span(write_buffer_, header_size));
      BufferWriteStream header_stream(header_buffer_stream);
      header_stream.Write(kHeader);
      header_stream.Write(config_.id);
      header_stream.Write(client_id);
      header_stream.WriteVaruint(response_size);
    }

    // Now figure out the checksum.
    const auto crc_location = header_size + response_size;
    boost::crc_ccitt_type crc;
    crc.process_bytes(write_buffer_, crc_location);
    const uint16_t actual_crc = crc.checksum();

    {
      base::BufferWriteStream crc_buffer_stream(
          base::string_span(&write_buffer_[crc_location], 2));
      BufferWriteStream crc_stream(crc_buffer_stream);
      crc_stream.Write(actual_crc);
    }

    write_outstanding_ = true;
    AsyncWrite(*stream_, std::string_view(write_buffer_, crc_location + 2),
               std::bind(&Impl::HandleWrite, this, std::placeholders::_1));
  }

  void HandleWrite(micro::error_code ec) {
    if (ec) {
      stats_.write_error++;
      stats_.last_write_error = ec.value();
    }
    write_outstanding_ = false;
  }

  void HandleWriteRaw(const micro::error_code& ec, size_t size) {
    if (ec) {
      stats_.write_error++;
      stats_.last_write_error = ec.value();
    }
    write_outstanding_ = false;
    auto callback = raw_write_callback_;
    raw_write_callback_ = {};
    callback(ec, size);
  }

  void ProcessSubframes(const std::string_view& subframes,
                        base::BufferWriteStream* response_buffer_stream,
                        BufferWriteStream* response_stream) {
    base::BufferReadStream buffer_stream(subframes);
    BufferReadStream str(buffer_stream);

    auto u8 = [](auto value) {
      return static_cast<uint8_t>(value);
    };

    struct RegisterHandler {
      uint8_t base_register = 0;
      bool (Impl::* handler)(uint8_t, BufferReadStream&, BufferWriteStream*);
    };

    constexpr RegisterHandler register_handlers[] = {
      { u8(Subframe::kWriteSingleBase), &Impl::ProcessSubframeWriteSingle },
      { u8(Subframe::kWriteMultipleBase), &Impl::ProcessSubframeWriteMultiple },
      { u8(Subframe::kReadSingleBase), &Impl::ProcessSubframeReadSingle },
      { u8(Subframe::kReadMultipleBase), &Impl::ProcessSubframeReadMultiple },
    };

    while (buffer_stream.remaining()) {
      const auto maybe_subframe_type = str.ReadVaruint();
      if (!maybe_subframe_type) {
        // The final subframe was malformed.  Guess we'll just ignore it.
        stats_.missing_subframe++;
        return;
      }

      const auto subframe_type = *maybe_subframe_type;

      if (subframe_type == u8(Subframe::kClientToServer)) {
        // The client sent us some data.
        if (ProcessSubframeClientToServer(
                buffer_stream, str,
                response_buffer_stream,
                response_stream)) {
          stats_.malformed_subframe++;
          return;
        }
        continue;
      }

      if (subframe_type == u8(Subframe::kClientPollServer)) {
        if (ProcessSubframeClientPollServer(
                buffer_stream, str,
                response_buffer_stream,
                response_stream)) {
          stats_.malformed_subframe++;
          return;
        }
        continue;
      }

      bool register_handler_found = false;
      for (const auto& handler : register_handlers) {
        if ((subframe_type & ~0x03) == handler.base_register) {
          if ((this->*handler.handler)(
                  subframe_type & 0x03, str, response_stream)) {
            stats_.malformed_subframe++;
            return;
          }
          register_handler_found = true;
          break;
        }
      }

      if (!register_handler_found) {
        // An unknown subframe.  Write off the rest of this frame as
        // unusable.
        stats_.unknown_subframe++;
        return;
      }
    }
  }

  // @return true if malformed
  bool ProcessSubframeClientToServer(
      base::BufferReadStream& buffer_stream,
      BufferReadStream& str,
      base::BufferWriteStream* response_buffer_stream,
      BufferWriteStream* response_stream) {
    const auto maybe_channel = str.ReadVaruint();
    const auto maybe_bytes = str.ReadVaruint();
    if (!maybe_channel || !maybe_bytes ||
        buffer_stream.remaining() < static_cast<std::streamsize>(*maybe_bytes)) {
      // Malformed.
      return true;
    }

    auto maybe_tunnel = FindTunnel(*maybe_channel);
    if (!maybe_tunnel) {
      return true;
    }

    auto& tunnel = *maybe_tunnel;

    const auto remaining_space =
        sizeof(tunnel.read_data_) - tunnel.read_data_size_;

    if (*maybe_bytes > remaining_space) {
      stats_.receive_overrun++;
    } else {
      str.base()->read({&tunnel.read_data_[tunnel.read_data_size_],
              static_cast<ssize_t>(*maybe_bytes)});
      tunnel.read_data_size_ += *maybe_bytes;
    }

    tunnel.DoReadTransfer();

    // Send our response if necessary.
    if (response_stream) {
      response_stream->WriteVaruint(static_cast<uint8_t>(Subframe::kServerToClient));
      response_stream->WriteVaruint(*maybe_channel);

      const ssize_t kExtraPadding = 16;
      const auto to_copy =
          std::min<std::streamsize>(
              tunnel.write_buffer_.size(),
              response_buffer_stream->remaining() -
              (kMaxVaruintSize + kCrcSize + kExtraPadding));
      response_stream->WriteVaruint(to_copy);
      if (to_copy > 0) {
        response_stream->base()->write(tunnel.write_buffer_.substr(0, to_copy));

        tunnel.write_buffer_ = {};
        auto cbk = tunnel.write_callback_;
        tunnel.write_callback_ = {};
        cbk({}, to_copy);
      }
    }

    return false;
  }

  bool ProcessSubframeClientPollServer(
      base::BufferReadStream&,
      BufferReadStream& str,
      base::BufferWriteStream* response_buffer_stream,
      BufferWriteStream* response_stream) {
    const auto maybe_channel = str.ReadVaruint();
    const auto maybe_max_bytes = str.ReadVaruint();
    if (!maybe_channel || !maybe_max_bytes) {
      // Malformed.
      return true;
    }

    auto maybe_tunnel = FindTunnel(*maybe_channel);
    if (!maybe_tunnel) {
      return true;
    }

    auto& tunnel = *maybe_tunnel;

    if (response_stream) {
      response_stream->WriteVaruint(static_cast<uint8_t>(Subframe::kServerToClient));
      response_stream->WriteVaruint(*maybe_channel);

      const ssize_t kExtraPadding = 16;
      const auto to_copy =
          std::min<std::streamsize>(
              *maybe_max_bytes,
              std::min<std::streamsize>(
                  tunnel.write_buffer_.size(),
                  response_buffer_stream->remaining() -
                  (kMaxVaruintSize + kCrcSize + kExtraPadding)));
      response_stream->WriteVaruint(to_copy);
      if (to_copy > 0) {
        response_stream->base()->write(tunnel.write_buffer_.substr(0, to_copy));

        tunnel.write_buffer_ = {};
        auto cbk = tunnel.write_callback_;
        tunnel.write_callback_ = {};
        cbk({}, to_copy);
      }
    }

    return false;
  }

  std::optional<Value> ReadValue(uint8_t type, BufferReadStream& str) {
    if (type == 0) {
      return str.ReadScalar<int8_t>();
    } else if (type == 1) {
      return str.ReadScalar<int16_t>();
    } else if (type == 2) {
      return str.ReadScalar<int32_t>();
    } else if (type == 3) {
      return str.ReadScalar<float>();
    }
    MJ_ASSERT(false);
    return {};
  }

  void EmitWriteError(BufferWriteStream* response,
                      Register error_reg, uint32_t error) {
    if (!response) { return; }
    response->WriteVaruint(static_cast<uint8_t>(Subframe::kWriteError));
    response->WriteVaruint(error_reg);
    response->WriteVaruint(error);
  }

  bool ProcessSubframeWriteSingle(uint8_t type, BufferReadStream& str,
                                  BufferWriteStream* response) {
    const auto maybe_register = str.ReadVaruint();
    if (!maybe_register) { return true; }

    const auto maybe_value = ReadValue(type, str);
    if (!maybe_value) { return true; }

    if (server_) {
      const auto error = server_->Write(*maybe_register, *maybe_value);
      if (error) {
        EmitWriteError(response, *maybe_register, error);
      }
    }

    return false;
  }

  bool ProcessSubframeWriteMultiple(uint8_t type, BufferReadStream& str,
                                    BufferWriteStream* response) {
    const auto start_register = str.ReadVaruint();
    if (!start_register) { return true; }

    const auto num_registers = str.ReadVaruint();
    if (!num_registers) { return true; }

    auto current_register = *start_register;

    for (size_t i = 0; i < *num_registers; i++) {
      const auto maybe_value = ReadValue(type, str);
      if (!maybe_value) { return true; }

      if (server_) {
        const auto error = server_->Write(current_register, *maybe_value);
        if (error) {
          EmitWriteError(response, current_register, error);
        }
      }
      current_register++;
    }

    return false;
  }

  void EmitReadResult(BufferWriteStream* response,
                      uint32_t reg,
                      const Value& value) {
    const uint8_t subframe_id = 0x20 | value.index();
    response->WriteVaruint(subframe_id);
    response->WriteVaruint(reg);
    std::visit([&](auto actual_value) {
        response->Write(actual_value);
      }, value);
  }

  void EmitReadError(BufferWriteStream* response,
                     uint32_t reg,
                     uint32_t error) {
    response->WriteVaruint(static_cast<uint8_t>(Subframe::kReadError));
    response->WriteVaruint(reg);
    response->WriteVaruint(error);
  }

  void EmitRead(BufferWriteStream* response,
                uint32_t reg,
                const ReadResult& read_result) {
    if (read_result.index() == 0) {
      EmitReadResult(response, reg, std::get<0>(read_result));
    } else {
      EmitReadError(response, reg, std::get<uint32_t>(read_result));
    }
  }

  bool ProcessSubframeReadSingle(uint8_t type, BufferReadStream& str,
                                 BufferWriteStream* response) {
    if (!response) { return false; }

    const auto maybe_register = str.ReadVaruint();
    if (!maybe_register) { return true; }

    const auto read_result =
        server_ ? server_->Read(*maybe_register, type) : uint32_t(1);
    EmitRead(response, *maybe_register, read_result);
    return false;
  }

  bool ProcessSubframeReadMultiple(uint8_t type, BufferReadStream& str,
                                   BufferWriteStream* response) {
    if (!response) { return false; }

    const auto start_register = str.ReadVaruint();
    if (!start_register) { return true; }

    const auto num_registers = str.ReadVaruint();
    if (!num_registers) { return true; }

    // Save our write position, in case we need to abort and emit an
    // error.
    auto* const start = response->base()->position();

    const uint8_t subframe_id = 0x24 | type;
    response->WriteVaruint(subframe_id);
    response->WriteVaruint(*start_register);
    response->WriteVaruint(*num_registers);

    auto current_register = *start_register;

    for (size_t i = 0; i < *num_registers; i++) {
      const auto read_result =
          server_ ? server_->Read(current_register, type) : uint32_t(1);

      if (read_result.index() == 0) {
        // We successfully read something.
        std::visit([&](auto actual_value) {
            response->Write(actual_value);
          }, std::get<0>(read_result));
      } else {
        // Nope, an error.  Pretend we wrote nothing, and instead emit
        // the read error we got back.
        response->base()->reset(start);
        EmitReadError(response, current_register,
                      std::get<uint32_t>(read_result));
        return true;
      }

      current_register++;
    }

    return true;
  }

  TunnelStream* FindTunnel(uint32_t id) {
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == id) { return &tunnel; }
    }
    return nullptr;
  }

  const Options options_;
  micro::AsyncStream* const stream_;
  Server* server_ = nullptr;

  Config config_;

  RawStream raw_write_stream_{this};

  std::streamsize read_start_ = 0;
  char* const read_buffer_ = {};
  bool read_outstanding_ = false;

  char* const write_buffer_ = {};
  bool write_outstanding_ = false;

  base::string_span unknown_buffer_;
  micro::SizeCallback unknown_callback_;

  micro::SizeCallback raw_write_callback_;

  TunnelStream tunnels_[1];
  Stats stats_;
};

MicroServer::MicroServer(
    micro::Pool* pool,
    micro::AsyncStream* async_stream,
    const Options& options)
    : impl_(pool, pool, async_stream, options) {}

MicroServer::~MicroServer() {}

micro::AsyncStream* MicroServer::MakeTunnel(uint32_t id) {
  return impl_->MakeTunnel(id);
}

void MicroServer::Start(Server* server) {
  impl_->Start(server);
}

void MicroServer::AsyncReadUnknown(const base::string_span& buffer,
                                   const micro::SizeCallback& callback) {
  impl_->AsyncReadUnknown(buffer, callback);
}

micro::AsyncWriteStream* MicroServer::raw_write_stream() {
  return impl_->raw_write_stream();
}

const MicroServer::Stats* MicroServer::stats() const {
  return impl_->stats();
}

MicroServer::Config* MicroServer::config() {
  return impl_->config();
}

}
}
