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

template <typename T>
constexpr uint8_t u8(T value) {
  return static_cast<uint8_t>(value);
}
}

class MicroServer::Impl {
 public:
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
       MicroDatagramServer* datagram_server,
       const Options& options)
      : options_(options),
        datagram_server_(datagram_server),
        datagram_properties_(datagram_server->properties()),
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

  const Stats* stats() const { return &stats_; }
  Config* config() { return &config_; }

 private:
  void MaybeStartReadFrame() {
    if (read_outstanding_) { return; }

    read_outstanding_ = true;
    datagram_server_->AsyncRead(
        &read_header_,
        base::string_span(read_buffer_, options_.buffer_size),
        std::bind(&Impl::HandleReadDatagram, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void HandleReadDatagram(const micro::error_code& ec, size_t) {
    read_outstanding_ = false;

    if (ec) {
      // We don't really have a way to log or do anything here.  So
      // lets just bail and start over.
      MaybeStartReadFrame();
      return;
    }

    ProcessFrame();

    MaybeStartReadFrame();
  }

  // Return true if more data might be available in the buffer.
  void ProcessFrame() __attribute__ ((optimize("O3"))) {
    if ((read_header_.destination == kBroadcastId) ||
        (read_header_.destination != (config_.id & 0x7f))) {
      stats_.wrong_id++;

      if (read_header_.destination != kBroadcastId) {
        return;
      }
    }

    base::BufferWriteStream buffer_write_stream{
      base::string_span(write_buffer_, options_.buffer_size)};
    BufferWriteStream write_stream{buffer_write_stream};
    const bool need_response =
        ((read_header_.source) & 0x80) != 0 &&
        !write_outstanding_;

    // Everything checked out.  Now we we can process our subframes.
    ProcessSubframes(
        std::string_view(read_buffer_, read_header_.size),
        &buffer_write_stream,
        need_response ? &write_stream : nullptr);

    if (need_response) {
      WriteResponse(read_header_.source & 0x7f, buffer_write_stream.offset());
    }
  }

  void WriteResponse(uint8_t client_id, std::streamsize response_size) {
    MJ_ASSERT(!write_outstanding_);

    write_header_.size = response_size;
    write_header_.source = config_.id & 0x7f;
    write_header_.destination = client_id;

    write_outstanding_ = true;
    datagram_server_->AsyncWrite(
        write_header_,
        std::string_view(write_buffer_, response_size),
        std::bind(&Impl::HandleWrite, this, std::placeholders::_1));
  }

  void HandleWrite(micro::error_code ec) {
    if (ec) {
      stats_.write_error++;
      stats_.last_write_error = ec.value();
    }
    write_outstanding_ = false;
  }

  void ProcessSubframes(const std::string_view& subframes,
                        base::BufferWriteStream* response_buffer_stream,
                        BufferWriteStream* response_stream)
      __attribute__ ((optimize("O3"))) {
    base::BufferReadStream buffer_stream(subframes);
    BufferReadStream str(buffer_stream);

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

      if (subframe_type == u8(Subframe::kNop)) {
        continue;
      }

      if (subframe_type >= u8(Subframe::kWriteBase) &&
          subframe_type < u8(Subframe::kWriteBase) + 16) {
        if (ProcessSubframeWrite(subframe_type - u8(Subframe::kWriteBase),
                                 str, response_stream)) {
          stats_.malformed_subframe++;
          return;
        }

        continue;
      }

      if (subframe_type >= u8(Subframe::kReadBase) &&
          subframe_type < u8(Subframe::kReadBase) + 16) {
        if (ProcessSubframeRead(subframe_type - u8(Subframe::kReadBase),
                                 str, response_stream)) {
          stats_.malformed_subframe++;
          return;
        }

        continue;
      }

      // An unknown subframe.  Write off the rest of this frame as
      // unusable.
      stats_.unknown_subframe++;
      return;
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
      const ssize_t kNeededOverhead =
          kMaxVaruintSize + kCrcSize + kExtraPadding;
      const auto to_copy =
          std::min<std::streamsize>(
              datagram_properties_.max_size - kNeededOverhead,
              std::min<std::streamsize>(
                  tunnel.write_buffer_.size(),
                  response_buffer_stream->remaining() - kNeededOverhead));
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
      const ssize_t kNeededOverhead =
          kMaxVaruintSize + kCrcSize + kExtraPadding;
      const auto to_copy =
          std::min<std::streamsize>(
              datagram_properties_.max_size - kNeededOverhead,
              std::min<std::streamsize>(
                  *maybe_max_bytes,
                  std::min<std::streamsize>(
                      tunnel.write_buffer_.size(),
                      response_buffer_stream->remaining() - kNeededOverhead)));
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

  bool ProcessSubframeWrite(uint8_t type_length,
                            BufferReadStream& str,
                            BufferWriteStream* response)
      __attribute__ ((optimize("O3"))) {
    const auto encoded_length = type_length % 4;
    const auto type = type_length / 4;

    const auto num_registers = (encoded_length == 0) ? str.ReadVaruint()
        : std::make_optional<uint32_t>(encoded_length);
    if (!num_registers) { return true; }

    const auto start_register = str.ReadVaruint();
    if (!start_register) { return true; }

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

  bool ProcessSubframeRead(uint8_t type_length,
                           BufferReadStream& str,
                           BufferWriteStream* response)
      __attribute__ ((optimize("O3"))) {
    if (!response) { return false; }

    const auto encoded_length = type_length % 4;
    const auto type = type_length / 4;

    const auto num_registers = (encoded_length == 0) ? str.ReadVaruint()
        : std::make_optional<uint32_t>(encoded_length);
    if (!num_registers) { return true; }


    const auto start_register = str.ReadVaruint();
    if (!start_register) { return true; }


    // Save our write position, in case we need to abort and emit an
    // error.
    auto* const start = response->base()->position();

    const uint8_t subframe_id =
        u8(Format::Subframe::kReplyBase) | (type * 4) | encoded_length;
    response->WriteVaruint(subframe_id);
    if (encoded_length == 0) {
      response->WriteVaruint(*num_registers);
    }
    response->WriteVaruint(*start_register);

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

    return false;
  }

  TunnelStream* FindTunnel(uint32_t id) {
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == id) { return &tunnel; }
    }
    return nullptr;
  }

  const Options options_;
  MicroDatagramServer* const datagram_server_;
  const MicroDatagramServer::Properties datagram_properties_;
  Server* server_ = nullptr;

  Config config_;

  MicroDatagramServer::Header read_header_;
  char* const read_buffer_ = {};
  bool read_outstanding_ = false;

  MicroDatagramServer::Header write_header_;
  char* const write_buffer_ = {};
  bool write_outstanding_ = false;

  TunnelStream tunnels_[1];
  Stats stats_;

  micro::AsyncWriter async_writer_;
};

MicroServer::MicroServer(
    micro::Pool* pool,
    MicroDatagramServer* datagram_server,
    const Options& options)
    : impl_(pool, pool, datagram_server, options) {}

MicroServer::~MicroServer() {}

micro::AsyncStream* MicroServer::MakeTunnel(uint32_t id) {
  return impl_->MakeTunnel(id);
}

void MicroServer::Start(Server* server) {
  impl_->Start(server);
}

const MicroServer::Stats* MicroServer::stats() const {
  return impl_->stats();
}

MicroServer::Config* MicroServer::config() {
  return impl_->config();
}

}
}
