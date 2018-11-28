// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include "mjlib/micro/multiplex_protocol.h"

#include <boost/crc.hpp>

#include "mjlib/base/assert.h"
#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/visitor.h"

namespace mjlib {
namespace micro {

namespace {
struct Config {
  uint8_t id = 1;

  template <typename Archive>
  void Serialize(Archive* a) {
    a->Visit(MJ_NVP(id));
  }
};

class ProtocolReadStream {
 public:
  ProtocolReadStream(base::BufferReadStream& istr) : istr_(istr) {}
  base::BufferReadStream* base() { return &istr_; }

  template <typename T>
  std::optional<T> Read() {
    return ReadScalar<T>();
  }

  std::optional<uint32_t> ReadVaruint() {
    uint32_t result = 0;
    int pos = 0;
    for (int i = 0; i < 5; i++) {
      if (istr_.remaining() == 0) { return {}; }
      const auto maybe_this_byte = Read<uint8_t>();
      if (!maybe_this_byte) {
        return {};
      }
      const auto this_byte = *maybe_this_byte;
      result |= (this_byte & 0x7f) << pos;
      pos += 7;
      if ((this_byte & 0x80) == 0) { return result; }
    }
    // Whoops, this was too big.  For now, just return maxint.
    return std::numeric_limits<uint32_t>::max();
  }

 private:
  template <typename T>
  std::optional<T> ReadScalar() {
    if (istr_.remaining() < static_cast<std::streamsize>(sizeof(T))) {
      return {};
    }

    T result = T{};
    RawRead(reinterpret_cast<char*>(&result), sizeof(result));
    return result;
  }

  void RawRead(char* out, std::streamsize size) {
    istr_.read({out, size});
    MJ_ASSERT(istr_.gcount() == size);
  }

  base::BufferReadStream& istr_;
};

}

class MultiplexProtocolServer::Impl {
 public:
  class TunnelStream : public AsyncStream {
   public:
    void set_parent(Impl* impl) { parent_ = impl; }
    uint32_t id() const { return id_; }
    void set_id(uint32_t id) { id_ = id; }

    void AsyncReadSome(const base::string_span& buffer,
                       const SizeCallback& callback) override {
      MJ_ASSERT(read_buffer_.empty());
      read_buffer_ = buffer;
      read_callback_ = callback;

      const bool complete = DoReadTransfer();
      if (complete) {
        // Consume the packet and try to process more.
        parent_->Consume(parent_->outstanding_consume_);
        parent_->outstanding_consume_ = 0;
        if (parent_->HandleMaybeFrame()) {
          parent_->MaybeStartReadFrame();
        }
      }
    }

    void AsyncWriteSome(const std::string_view&, const SizeCallback&) override {
    }

    bool DoReadTransfer() {
      if (read_buffer_.empty() ||
          outstanding_read_data_.empty()) {
        return false;
      }

      const auto to_copy = std::min<std::streamsize>(
          read_buffer_.size(), outstanding_read_data_.size());
      std::memcpy(read_buffer_.data(), outstanding_read_data_.data(), to_copy);
      outstanding_read_data_ =
          std::string_view(&outstanding_read_data_[to_copy],
                           outstanding_read_data_.size() - to_copy);
      read_buffer_ = {};
      const bool complete = outstanding_read_data_.size() == 0;

      auto callback = read_callback_;
      read_callback_ = {};

      callback({}, to_copy);

      return complete;
    }

    Impl* parent_ = nullptr;
    uint32_t id_ = 0;

    base::string_span read_buffer_;
    SizeCallback read_callback_;
    std::string_view outstanding_read_data_;

    std::string_view write_buffer_;
    SizeCallback write_callback_;
  };

  Impl(Pool* pool,
       PersistentConfig* persistent_config,
       AsyncStream* stream,
       Server*,
       const Options& options)
      : options_(options),
        stream_(stream),
        read_buffer_(static_cast<char*>(
                         pool->Allocate(options.buffer_size, 1))) {
    persistent_config->Register("protocol", &config_, [](){});

    for (auto& tunnel : tunnels_) {
      tunnel.set_parent(this);
    }
  }

  AsyncStream* MakeTunnel(uint32_t id) {
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

  void Start() {
    MaybeStartReadFrame();
  }

  const Stats* stats() const { return &stats_; }

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

  void HandleReadFrame(const base::error_code& ec, size_t size) {
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

    const bool read_again = HandleMaybeFrame();
    if (read_again) {
      MaybeStartReadFrame();
    }
  }

  bool HandleMaybeFrame() {
    // For now, we'll require that we fit a whole valid packet into
    // our buffer at once before acting on it.  So we'll just keep
    // calling StartReadFrame until we get there.

    // Work to start our buffer out with the frame header.

    while (true) {
      char* const found = static_cast<char*>(
          std::memchr(read_buffer_, (kHeader & 0xff), read_start_));
      if (found == nullptr) {
        // We don't have anything which could be a header in our
        // buffer.  Just wipe it all out and start over.
        read_start_ = 0;
        return true;
      }

      Consume(found - read_buffer_);

      if (read_start_ < 2) {
        // We need more to even have a header.
        return true;
      }

      if (static_cast<uint8_t>(read_buffer_[1]) != ((kHeader >> 8) & 0xff)) {
        // We had the first byte of a header, but not the second byte.

        // Move out this false start and try again.
        Consume(2);
        continue;
      }

      // Looks like we have a valid frame start.
      break;
    }

    // We need at least 7 bytes to have a minimal frame.
    if (read_start_ < 7) { return true; }

    // See if we have enough data to have a valid varuint for size.
    base::BufferReadStream data({&read_buffer_[2],
            static_cast<size_t>(read_start_ - 2)});
    ProtocolReadStream read_stream{data};

    const auto maybe_source_id = read_stream.Read<uint8_t>();
    (void)maybe_source_id;
    const auto maybe_dest_id = read_stream.Read<uint8_t>();
    (void)maybe_dest_id;
    const auto maybe_size = read_stream.ReadVaruint();

    if (!maybe_size) {
      // We don't have enough for the size yet.
      return true;
    }

    const uint32_t payload_size = *maybe_size;
    if (payload_size > 4096) {
      // We'll claim this is guaranteed to be too big.  Just wipe
      // everything out and start over.
      read_start_ = 0;
      return true;
    }

    if (payload_size > (options_.buffer_size - 7))  {
      // We can't fit this either.
      read_start_ = 0;
      return true;
    }

    // Do we have enough data yet?
    const char* const payload_start = read_stream.base()->position();
    const auto data_we_have =
        &read_buffer_[read_start_] - read_stream.base()->position();
    if (data_we_have < payload_size + 2) {
      // We need more still.
      return true;
    }

    const char* const crc_location =
        read_stream.base()->position() + payload_size;

    // Woohoo.  We nominally have enough for a whole frame.  Verify
    // the checksum!
    boost::crc_16_type crc;
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

    if (*maybe_dest_id != config_.id) {
      stats_.wrong_id++;
      Consume(read_stream.base()->position() - read_buffer_);
      return true;
    }

    // Wow, everything checked out.  Now we we can process our subframes.
    const bool complete = ProcessSubframes(
        std::string_view(payload_start, payload_size));
    const auto to_consume = crc_location - read_buffer_ + 2;
    if (!complete) {
      outstanding_consume_ = to_consume;
    } else {
      outstanding_consume_ = 0;
      Consume(to_consume);
    }
    return complete;
  }

  void Consume(std::streamsize size) {
    MJ_ASSERT(read_start_ >= size);
    if (size == 0) { return; }
    std::memmove(read_buffer_, &read_buffer_[size], read_start_ - size);
    read_start_ -= size;
  }

  bool ProcessSubframes(const std::string_view& subframes) {
    base::BufferReadStream buffer_stream(subframes);
    ProtocolReadStream str(buffer_stream);

    while (buffer_stream.remaining()) {
      const auto maybe_subframe_type = str.ReadVaruint();
      if (!maybe_subframe_type) {
        // The final subframe was malformed.  Guess we'll just ignore it.
        return true;
      }

      const auto subframe_type = *maybe_subframe_type;

      if (subframe_type == 0x40) {
        // The client sent us some data.
        const auto error = ProcessSubframeClientToServer(str);
        switch (error) {
          case kSuccess: { break; }
          case kError: { return true; }
          case kIncomplete: { return false; }
        }
      } else {
        // An unknown subframe.  Write off the rest of this frame as
        // unusable.
        return true;
      }
    }

    return true;
  }

  enum ClientToServerResult {
    kSuccess,
    kIncomplete,
    kError,
  };

  ClientToServerResult ProcessSubframeClientToServer(ProtocolReadStream& str) {
    const auto maybe_channel = str.ReadVaruint();
    const auto maybe_bytes = str.ReadVaruint();
    if (!maybe_channel || !maybe_bytes ||
        str.base()->remaining() < *maybe_bytes) {
      // Malformed.
      return kError;
    }

    auto maybe_tunnel = FindTunnel(*maybe_channel);
    if (!maybe_tunnel) {
      return kError;
    }

    auto& tunnel = *maybe_tunnel;
    tunnel.outstanding_read_data_ =
        std::string_view(str.base()->position(), *maybe_bytes);

    const bool complete = tunnel.DoReadTransfer();
    return complete ? kSuccess : kIncomplete;
  }

  TunnelStream* FindTunnel(uint32_t id) {
    for (auto& tunnel : tunnels_) {
      if (tunnel.id() == id) { return &tunnel; }
    }
    return nullptr;
  }

  const Options options_;
  AsyncStream* const stream_;

  Config config_;

  std::streamsize read_start_ = 0;
  char* const read_buffer_ = {};
  bool read_outstanding_ = false;
  ssize_t outstanding_consume_ = 0;

  TunnelStream tunnels_[1] = {};
  Stats stats_;
};

MultiplexProtocolServer::MultiplexProtocolServer(
    Pool* pool,
    PersistentConfig* persistent_config,
    AsyncStream* async_stream,
    Server* server,
    const Options& options)
    : impl_(pool, pool, persistent_config, async_stream, server, options) {}

MultiplexProtocolServer::~MultiplexProtocolServer() {}

AsyncStream* MultiplexProtocolServer::MakeTunnel(uint32_t id) {
  return impl_->MakeTunnel(id);
}

void MultiplexProtocolServer::Start() {
  impl_->Start();
}

const MultiplexProtocolServer::Stats* MultiplexProtocolServer::stats() const {
  return impl_->stats();
}

}
}
