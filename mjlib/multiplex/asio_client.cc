// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/asio_client.h"

#include <functional>

#include <boost/asio/write.hpp>
#include <boost/crc.hpp>

#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/io/deadline_timer.h"
#include "mjlib/io/exclusive_command.h"
#include "mjlib/io/offset_buffer.h"

#include "mjlib/multiplex/frame_stream.h"
#include "mjlib/multiplex/stream.h"

namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {
namespace {
const boost::posix_time::time_duration kDefaultTimeout =
    boost::posix_time::milliseconds(10);

template <typename T>
uint32_t u32(T value) {
  return static_cast<uint32_t>(value);
}
}

class AsioClient::Impl {
 public:
  Impl(io::AsyncStream* stream, const Options& options)
      : stream_(stream),
        options_(options),
        frame_stream_(stream_) {}

  void AsyncRegister(uint8_t id,
                     const RegisterRequest& request,
                     RegisterHandler handler) {
    lock_.Invoke([this, id, request](RegisterHandler handler) {
        tx_frame_.source_id = this->options_.source_id;
        tx_frame_.dest_id = id;
        tx_frame_.request_reply = request.request_reply();
        tx_frame_.payload = request.buffer();

        frame_stream_.AsyncWrite(
            &tx_frame_, std::bind(&Impl::HandleWrite, this, pl::_1,
                                  handler, request.request_reply()));
      },
      handler);
  }

  void HandleWrite(const base::error_code& ec,
                   RegisterHandler handler,
                   bool request_reply) {
    if (!request_reply) {
      service_.post(std::bind(handler, ec, RegisterReply()));
      return;
    }

    FailIf(ec);

    frame_stream_.AsyncRead(
        &rx_frame_, kDefaultTimeout,
        std::bind(&Impl::HandleRead, this, pl::_1, handler));
  }

  void HandleRead(const base::error_code& ec, RegisterHandler handler) {
    base::FailIf(ec);

    // If this isn't from who we expected, just read again.
    if (rx_frame_.source_id != tx_frame_.dest_id ||
        rx_frame_.dest_id != tx_frame_.source_id) {
      frame_stream_.AsyncRead(
          &rx_frame_, kDefaultTimeout,
          std::bind(&Impl::HandleRead, this, pl::_1, handler));
      return;
    }

    base::FastIStringStream stream(rx_frame_.payload);
    auto reply = ParseRegisterReply(stream);
    service_.post(std::bind(handler, ec, reply));
  }

  io::SharedStream MakeTunnel(uint8_t id, uint32_t channel,
                              const TunnelOptions& options) {
    return std::make_shared<Tunnel>(this, id, channel, options);
  }

 private:
  class Tunnel : public io::AsyncStream {
   public:
    Tunnel(Impl* parent, uint8_t id, uint32_t channel,
           const TunnelOptions& options)
        : parent_(parent),
          id_(id),
          channel_(channel),
          options_(options) {}

    ~Tunnel() override {}

    void async_read_some(io::MutableBufferSequence buffers,
                         io::ReadHandler handler) override {
      // We create a local lambda to be the immediate callback, that
      // way the lock can be released in between polls.  Boy would
      // this ever be more clear if we could express it with
      // coroutines.
      auto maybe_retry = [this, buffers, handler](const base::error_code& ec,
                                                  size_t bytes_read) {
        base::FailIf(ec);

        if (parent_->frame_stream_.read_data_queued() &&
            bytes_read < boost::asio::buffer_size(buffers)) {
          // Hmmm, we have more data available and room to put it.
          // This isn't possible unless either a slave was talking in
          // an unsolicited manner, or we actually got a response to a
          // previous request that had timed out, and our response is
          // still somewhere in the queue.  In any event, we'll want
          // to keep reading this to flush any data still on the
          // stream.

          auto handler_wrapper =
              [handler, bytes_read](const base::error_code& ec, size_t size) {
            handler(ec, bytes_read + size);
          };

          this->async_read_some(
              io::OffsetBufferSequence(buffers, bytes_read),
              handler_wrapper);

          return;
        }

        if (bytes_read > 0) {
          // No need to retry, we're done.
          parent_->service_.post(
              std::bind(handler, base::error_code(), bytes_read));
          return;
        }

        // Yep, we failed to get anything this time.  Wait our polling
        // period and try again.
        poll_timer_.expires_from_now(options_.poll_rate);
        poll_timer_.async_wait([this, buffers, handler](auto&& ec) {
            if (ec == boost::asio::error::operation_aborted) { return; }
            this->async_read_some(buffers, handler);
          });
      };

      parent_->lock_.Invoke(
          [this, buffers](io::SizeCallback handler) {
            MakeFrame(boost::asio::buffer("", 0), true);

            parent_->frame_stream_.AsyncWrite(
                &parent_->tx_frame_,
                std::bind(&Tunnel::HandleRequestRead, this, pl::_1,
                          buffers, handler));
          },
          maybe_retry);
    }

    void async_write_some(io::ConstBufferSequence buffers,
                          io::WriteHandler handler) override {
      parent_->lock_.Invoke([this, buffers](io::WriteHandler handler) {
          const auto size = boost::asio::buffer_size(buffers);
          MakeFrame(buffers, false);

          parent_->frame_stream_.AsyncWrite(
              &parent_->tx_frame_,
              [handler, size](auto&& ec) {
                if (ec) {
                  handler(ec, 0);
                } else {
                  handler(ec, size);
                }
              });
        },
        handler);
    }

    boost::asio::io_service& get_io_service() override {
      return parent_->service_;
    }

    void cancel() override {
    }

   private:
    void HandleRequestRead(const base::error_code& ec,
                           io::MutableBufferSequence buffers,
                           io::SizeCallback callback) {
      base::FailIf(ec);

      // Now we need to try and read the response.
      parent_->frame_stream_.AsyncRead(
          &parent_->rx_frame_,
          kDefaultTimeout,
          std::bind(&Tunnel::HandleRead, this, pl::_1, buffers, callback));
    }

    void HandleRead(const base::error_code& ec, io::MutableBufferSequence buffers,
                    io::SizeCallback callback) {
      if (ec == boost::asio::error::operation_aborted) {
        // We got a timeout.  Just wait our polling period and try again.
        callback({}, 0u);
        return;
      }

      // Verify the response came from who we expected it to, and was
      // addressed to us.
      auto& frame = parent_->rx_frame_;
      if (frame.source_id != this->id_ ||
          frame.dest_id != parent_->options_.source_id) {
        // Just retry.
        callback({}, 0u);
        return;
      }

      // Now, parse the response.
      base::FastIStringStream stream(frame.payload);
      ReadStream reader{stream};

      // For basically any error, we're just going to retry.
      const auto maybe_subframe = reader.ReadVaruint();
      if (!maybe_subframe) {
        callback({}, 0u);
        return;
      }

      if (*maybe_subframe != u32(Format::Subframe::kServerToClient)) {
        callback({}, 0u);
        return;
      }

      const auto maybe_channel = reader.ReadVaruint();
      if (!maybe_channel) {
        callback({}, 0u);
        return;
      }
      if (*maybe_channel != channel_) {
        callback({}, 0u);
        return;
      }

      const auto maybe_size = reader.ReadVaruint();
      if (!maybe_size) {
        callback({}, 0u);
        return;
      }

      if (*maybe_size > stream.remaining()) {
        callback({}, 0u);
        return;
      }

      // OK, we've got something.
      boost::asio::buffer_copy(
          buffers, boost::asio::buffer(&frame.payload[stream.offset()],
                                       *maybe_size));
      callback({}, *maybe_size);
    }

    void MakeFrame(io::ConstBufferSequence buffers, bool request_reply) {
      base::FastOStringStream stream;
      WriteStream writer{stream};

      writer.WriteVaruint(u32(Format::Subframe::kClientToServer));
      writer.WriteVaruint(channel_);

      const auto size = boost::asio::buffer_size(buffers);
      writer.WriteVaruint(size);
      temp_data_.resize(size);
      boost::asio::buffer_copy(boost::asio::buffer(&temp_data_[0], size), buffers);
      stream.write(std::string_view(&temp_data_[0], size));

      Frame& frame = parent_->tx_frame_;
      frame.source_id = parent_->options_.source_id;
      frame.dest_id = id_;
      frame.request_reply = request_reply;
      frame.payload = stream.str();
    }

    Impl* const parent_;
    const uint8_t id_;
    const uint32_t channel_;
    const TunnelOptions options_;

    io::DeadlineTimer poll_timer_{parent_->service_};
    std::vector<char> temp_data_;
  };

  io::AsyncStream* const stream_;
  boost::asio::io_service& service_{stream_->get_io_service()};
  const Options options_;
  FrameStream frame_stream_;

  io::ExclusiveCommand lock_{service_};

  Frame rx_frame_;
  Frame tx_frame_;
};

AsioClient::AsioClient(io::AsyncStream* stream, const Options& options)
    : impl_(std::make_unique<Impl>(stream, options)) {}

AsioClient::~AsioClient() {}

void AsioClient::AsyncRegister(uint8_t id,
                               const RegisterRequest& request,
                               RegisterHandler handler) {
  impl_->AsyncRegister(id, request, handler);
}

io::SharedStream AsioClient::MakeTunnel(
    uint8_t id, uint32_t channel, const TunnelOptions& options) {
  return impl_->MakeTunnel(id, channel, options);
}

}
}
