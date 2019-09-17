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

#include <boost/asio/post.hpp>
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

  void AsyncRegisterMultiple(const std::vector<IdRequest>& requests,
                             io::ErrorCallback handler) {
    lock_.Invoke([this, requests](io::ErrorCallback handler) {
        tx_frames_.resize(requests.size());
        tx_frame_ptrs_.clear();

        for (size_t i = 0; i < requests.size(); i++) {
          tx_frames_[i].source_id = this->options_.source_id;
          tx_frames_[i].dest_id = requests[i].id;
          BOOST_ASSERT(!requests[i].request.request_reply());
          tx_frames_[i].request_reply = false;
          tx_frames_[i].payload = requests[i].request.buffer();
          tx_frame_ptrs_.push_back(&tx_frames_[i]);
        }

        RegisterHandler reg_handler = [handler](
            const base::error_code& ec,
            const RegisterReply&) {
          handler(ec);
        };

        frame_stream_.AsyncWriteMultiple(
            tx_frame_ptrs_, std::bind(
                &Impl::HandleWrite, this, pl::_1,
                reg_handler, false));
      },
      handler);
  }

  void HandleWrite(const base::error_code& ec,
                   RegisterHandler handler,
                   bool request_reply) {
    if (!request_reply) {
      boost::asio::post(
          executor_,
          std::bind(handler, ec, RegisterReply()));
      return;
    }

    FailIf(ec);

    frame_stream_.AsyncRead(
        &rx_frame_, kDefaultTimeout,
        std::bind(&Impl::HandleRead, this, pl::_1, handler));
  }

  void HandleRead(const base::error_code& ec, RegisterHandler handler) {
    // If we got a timeout, report that upstream.
    if (ec == boost::asio::error::operation_aborted) {
      boost::asio::post(
          executor_,
          std::bind(handler, ec, RegisterReply()));
      return;
    }

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
    boost::asio::post(
        executor_,
        std::bind(handler, ec, reply));
  }

  io::SharedStream MakeTunnel(uint8_t id, uint32_t channel,
                              const TunnelOptions& options) {
    return std::make_shared<TunnelHolder>(this, id, channel, options);
  }

 private:
  class Tunnel : public io::AsyncStream,
                 public std::enable_shared_from_this<Tunnel> {
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
      MJ_ASSERT(!read_handler_);

      // We create a local lambda to be the immediate callback, that
      // way the lock can be released in between polls.  Boy would
      // this ever be more clear if we could express it with
      // coroutines.

      read_handler_ = handler;
      read_bytes_read_ = 0;

      read_nonce_ = parent_->lock_.Invoke(
          [self=shared_from_this(), buffers](io::SizeCallback handler) {
            self->read_nonce_ = {};
            self->MakeFrame(boost::asio::buffer("", 0), true);

            self->parent_->frame_stream_.AsyncWrite(
                &self->parent_->tx_frame_,
                std::bind(&Tunnel::HandleRequestRead, self, pl::_1,
                          buffers, handler));
          },
          std::bind(&Tunnel::MaybeRetry, shared_from_this(),
                    pl::_1, pl::_2, buffers));
    }

    void async_write_some(io::ConstBufferSequence buffers,
                          io::WriteHandler handler) override {
      MJ_ASSERT(!write_handler_);
      write_handler_ = handler;
      write_nonce_ = parent_->lock_.Invoke(
          [self=shared_from_this(), buffers](io::WriteHandler handler) {
            self->write_nonce_ = {};
            self->write_handler_ = {};
            const auto size = boost::asio::buffer_size(buffers);
            self->MakeFrame(buffers, false);

            self->parent_->frame_stream_.AsyncWrite(
                &self->parent_->tx_frame_,
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

    boost::asio::executor get_executor() override {
      return parent_->executor_;
    }

    void cancel() override {
      parent_->lock_.remove(read_nonce_);
      poll_timer_.cancel();
      parent_->frame_stream_.cancel();

      if (read_handler_) {
        boost::asio::post(
            get_executor(),
            std::bind(read_handler_,
                      boost::asio::error::operation_aborted, 0));
      }
      read_handler_ = {};
      read_bytes_read_ = 0;

      if (parent_->lock_.remove(write_nonce_)) {
        boost::asio::post(
            get_executor(),
            std::bind(write_handler_,
                      boost::asio::error::operation_aborted, 0));
      }
      write_handler_ = {};

      read_nonce_ = {};
      write_nonce_ = {};
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
          std::bind(&Tunnel::HandleRead, shared_from_this(),
                    pl::_1, buffers, callback));
    }

    void HandleRead(const base::error_code& ec,
                    io::MutableBufferSequence buffers,
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

      read_bytes_read_ += *maybe_size;

      // See if there is more waiting to be flushed out, in which
      // case, get it all before relinquishing to our final callback.
      if (parent_->frame_stream_.read_data_queued() &&
          *maybe_size < boost::asio::buffer_size(buffers)) {
        // Hmmm, we have more data available and room to put it.
        // This isn't possible unless either a slave was talking in
        // an unsolicited manner, or we actually got a response to a
        // previous request that had timed out, and our response is
        // still somewhere in the queue.  In any event, we'll want
        // to keep reading this to flush any data still on the
        // stream.

        auto offset_buffers = io::OffsetBufferSequence(buffers, *maybe_size);

        HandleRequestRead(base::error_code(),
                          offset_buffers,
                          callback);
        return;
      }


      callback({}, read_bytes_read_);
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

    void MaybeRetry(const base::error_code& ec, size_t,
                    io::MutableBufferSequence buffers) {
      base::FailIf(ec);
      if (!read_handler_) {
        return;
      }

      if (read_bytes_read_ > 0) {
        // No need to retry, we're done.
        boost::asio::post(
            parent_->executor_,
            std::bind(read_handler_, base::error_code(), read_bytes_read_));
        read_handler_ = {};
        read_bytes_read_ = 0;
        return;
      }

      // Yep, we failed to get anything this time.  Wait our polling
      // period and try again.
      poll_timer_.expires_from_now(options_.poll_rate);
      poll_timer_.async_wait([self = shared_from_this(), buffers](auto&& ec) {
          if (ec == boost::asio::error::operation_aborted) { return; }
          auto copy = self->read_handler_;
          self->read_handler_ = {};
          self->read_bytes_read_ = 0;
          self->async_read_some(buffers, copy);
        });
    }

    Impl* const parent_;
    const uint8_t id_;
    const uint32_t channel_;
    const TunnelOptions options_;

    io::DeadlineTimer poll_timer_{parent_->executor_};
    std::vector<char> temp_data_;

    io::ExclusiveCommand::Nonce read_nonce_;
    io::ExclusiveCommand::Nonce write_nonce_;
    io::WriteHandler write_handler_;
    io::ReadHandler read_handler_;
    std::size_t read_bytes_read_ = 0;
  };

  class TunnelHolder : public io::AsyncStream {
   public:
    TunnelHolder(Impl* parent, uint8_t id, uint32_t channel,
                 const TunnelOptions& options)
        : impl_(std::make_shared<Tunnel>(parent, id, channel, options)) {}

    ~TunnelHolder() {
      cancel();
    }

    void async_read_some(io::MutableBufferSequence buffers,
                         io::ReadHandler handler) override {
      return impl_->async_read_some(buffers, handler);
    }

    void async_write_some(io::ConstBufferSequence buffers,
                          io::WriteHandler handler) override {
      return impl_->async_write_some(buffers, handler);
    }

    boost::asio::executor get_executor() override {
      return impl_->get_executor();
    }

    void cancel() override {
      impl_->cancel();
    }

   private:
    std::shared_ptr<Tunnel> impl_;
  };

  io::AsyncStream* const stream_;
  boost::asio::executor executor_{stream_->get_executor()};
  const Options options_;
  FrameStream frame_stream_;

  io::ExclusiveCommand lock_{executor_};

  Frame rx_frame_;
  Frame tx_frame_;
  std::vector<Frame> tx_frames_;
  std::vector<const Frame*> tx_frame_ptrs_;
};

AsioClient::AsioClient(io::AsyncStream* stream, const Options& options)
    : impl_(std::make_unique<Impl>(stream, options)) {}

AsioClient::~AsioClient() {}

void AsioClient::AsyncRegister(uint8_t id,
                               const RegisterRequest& request,
                               RegisterHandler handler) {
  impl_->AsyncRegister(id, request, handler);
}

void AsioClient::AsyncRegisterMultiple(
    const std::vector<IdRequest>& requests,
    io::ErrorCallback handler) {
  impl_->AsyncRegisterMultiple(requests, handler);
}

io::SharedStream AsioClient::MakeTunnel(
    uint8_t id, uint32_t channel, const TunnelOptions& options) {
  return impl_->MakeTunnel(id, channel, options);
}

}
}
