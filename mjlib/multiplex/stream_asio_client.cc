// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/multiplex/stream_asio_client.h"

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
    boost::posix_time::milliseconds(15);

template <typename T>
uint32_t u32(T value) {
  return static_cast<uint32_t>(value);
}

struct ReadContext {
  io::ReadHandler handler;
  io::MutableBufferSequence buffers;
  bool canceled = false;
  size_t bytes_read = 0;
};
}

class StreamAsioClient::Impl {
 public:
  Impl(FrameStream* frame_stream, const Options& options)
      : options_(options),
        frame_stream_(*frame_stream) {}

  void AsyncTransmit(const Request* request,
                     Reply* reply,
                     io::ErrorCallback handler) {
    // If any of the requests need a response, then execute them one
    // after the other sequentially.
    const bool any_replies = [&]() {
      for (const auto& id_request : *request) {
        if (id_request.request.request_reply()) { return true; }
      }
      return false;
    }();

    if (reply) { reply->clear(); }

    if (any_replies) {
      SequenceRegister(*request, reply, std::move(handler));
    } else {
      // No replies, we can just send this out as one big block with
      // no reads whatsoever.
      lock_.Invoke([this, request](io::ErrorCallback handler_in) mutable {
          tx_frames_.resize(request->size());
          tx_frame_ptrs_.clear();

          for (size_t i = 0; i < request->size(); i++) {
            tx_frames_[i].source_id = this->options_.source_id;
            tx_frames_[i].dest_id = (*request)[i].id;
            BOOST_ASSERT(!(*request)[i].request.request_reply());
            tx_frames_[i].request_reply = false;
            tx_frames_[i].payload = (*request)[i].request.buffer();
            tx_frame_ptrs_.push_back(&tx_frames_[i]);
          }

          frame_stream_.AsyncWriteMultiple(tx_frame_ptrs_, std::move(handler_in));
      },
      std::move(handler));
    }
  }

  void SequenceRegister(const Request& request, Reply* reply,
                        io::ErrorCallback callback) {
    if (request.empty()) {
      boost::asio::post(
          executor_,
          std::bind(std::move(callback), base::error_code()));
      return;
    }

    auto this_request = request.front();
    auto remainder = std::vector<IdRequest>{request.begin() + 1, request.end()};

    auto next = [this, handler=std::move(callback), reply, remainder](
        const base::error_code& ec) mutable {
      if (ec) {
        boost::asio::post(
            executor_,
            std::bind(std::move(handler), ec));
      } else {
        this->SequenceRegister(remainder, reply, std::move(handler));
      }
    };

    // Do the first one in the list.
    AsyncRegister(this_request, reply, std::move(next));
  }

  void AsyncRegister(const IdRequest& request, Reply* reply,
                     io::ErrorCallback handler) {
    lock_.Invoke([this, request, reply](io::ErrorCallback handler_in) mutable {
        tx_frame_.source_id = this->options_.source_id;
        tx_frame_.dest_id = request.id;
        const bool request_reply = request.request.request_reply();
        tx_frame_.request_reply = request_reply;
        tx_frame_.payload = request.request.buffer();

        frame_stream_.AsyncWrite(
            &tx_frame_,
            [this, handler_in=std::move(handler_in), reply, request_reply](
                const auto& ec) mutable {
              this->HandleSingleWrite(
                  ec, std::move(handler_in), reply, request_reply);
            });
      },
      std::move(handler));
  }

  void HandleSingleWrite(const base::error_code& ec,
                         io::ErrorCallback handler,
                         Reply* reply,
                         bool request_reply) {
    if (!request_reply) {
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), ec));
      return;
    }

    base::FailIf(ec);

    frame_stream_.AsyncRead(
        &rx_frame_, kDefaultTimeout,
        [this, reply, handler=std::move(handler)](const auto& ec) mutable {
          this->HandleRead(ec, reply, std::move(handler));
        });
  }


  void HandleRead(const base::error_code& ec, Reply* reply,
                  io::ErrorCallback handler) {
    // If we got a timeout, report that upstream.
    if (ec == boost::asio::error::operation_aborted) {
      boost::asio::post(
          executor_,
          std::bind(std::move(handler), ec));
      return;
    }

    base::FailIf(ec);

    // If this isn't from who we expected, just read again.
    if (rx_frame_.source_id != tx_frame_.dest_id ||
        rx_frame_.dest_id != tx_frame_.source_id) {
      frame_stream_.AsyncRead(
          &rx_frame_, kDefaultTimeout,
          [this, reply, handler=std::move(handler)](const auto& ec) mutable {
            this->HandleRead(ec, reply, std::move(handler));
          });
      return;
    }

    base::FastIStringStream stream(rx_frame_.payload);
    if (reply) {
      parsed_values_.clear();
      ParseRegisterReply(stream, &parsed_values_);
      for (const auto& parsed_value : parsed_values_) {
        reply->push_back(
            {rx_frame_.source_id,
                  parsed_value.first,
                  parsed_value.second});
      }
    }
    boost::asio::post(
        executor_,
        std::bind(std::move(handler), ec));
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
      MJ_ASSERT(!read_context_);

      if (boost::asio::buffer_size(buffers) == 0) {
        // We're done!
        boost::asio::post(
            get_executor(),
            std::bind(std::move(handler), base::error_code(), 0));
        return;
      }

      auto ctx = std::make_shared<ReadContext>();
      ctx->handler = std::move(handler);
      ctx->buffers = std::move(buffers);
      read_context_ = ctx;

      // We create a local lambda to be the immediate callback, that
      // way the lock can be released in between polls.  Boy would
      // this ever be more clear if we could express it with
      // coroutines.

      read_nonce_ = parent_->lock_.Invoke(
          [self=shared_from_this(), ctx](io::SizeCallback handler) mutable {
            self->read_nonce_ = {};

            // Have we been canceled?
            if (ctx->canceled) {
              ctx->handler(boost::asio::error::operation_aborted, 0);
              return;
            }

            self->MakeFrame(boost::asio::buffer("", 0), 0, true);

            self->parent_->frame_stream_.AsyncWrite(
                &self->parent_->tx_frame_,
                [self, handler=std::move(handler), ctx](const auto& ec) mutable {
                  self->HandleRequestRead(ec, std::move(handler), ctx);
                });
          },
          [self=shared_from_this(), ctx](auto&& _1, auto&& _2) {
            self->MaybeRetry(_1, _2, ctx);
          });
    }

    void async_write_some(io::ConstBufferSequence buffers,
                          io::WriteHandler handler) override {
      MJ_ASSERT(!write_handler_);

      if (boost::asio::buffer_size(buffers) == 0) {
        boost::asio::post(
            get_executor(),
            std::bind(std::move(handler), base::error_code(), 0));
        return;
      }

      write_handler_ = std::move(handler);
      write_nonce_ = parent_->lock_.Invoke(
          [self=shared_from_this(), buffers](
              io::WriteHandler inner_handler) mutable {
            auto size = boost::asio::buffer_size(buffers);
            if (self->parent_->stream_properties_.max_size >= 0) {
              const int kVarUintSize = 1;
              const int kNeededOverhead = 3 * kVarUintSize;

              BOOST_ASSERT(kNeededOverhead <=
                           self->parent_->stream_properties_.max_size);
              size = std::min<size_t>(
                  size, self->parent_->stream_properties_.max_size -
                  kNeededOverhead);
            }
            self->MakeFrame(buffers, size, false);

            self->parent_->frame_stream_.AsyncWrite(
                &self->parent_->tx_frame_,
                [inner_handler = std::move(inner_handler), size](
                    auto&& ec) mutable {
                  if (ec) {
                    inner_handler(ec, 0);
                  } else {
                    inner_handler(ec, size);
                  }
                });
          },
          [this](auto&& _1, auto&& _2) {
            auto copy = std::move(this->write_handler_);
            this->write_handler_ = {};
            this->write_nonce_ = {};
            copy(_1, _2);
          });
    }

    boost::asio::any_io_executor get_executor() override {
      return parent_->executor_;
    }

    void cancel() override {
      if (read_context_) {
        read_context_->canceled = true;
      }

      // See if we have yet to be executed at all.
      if (parent_->lock_.remove(read_nonce_)) {
        read_nonce_ = {};
        MJ_ASSERT(read_context_);
        boost::asio::post(
            get_executor(),
            std::bind(std::move(read_context_->handler),
                      boost::asio::error::operation_aborted, 0));
      }

      // Canceling the timer (or having its callback already enqueued
      // with the context->canceled set), will result in our read
      // handler being invoked with the 'operation_aborted' error
      // code.
      poll_timer_.cancel();
      read_context_ = {};

      if (parent_->lock_.remove(write_nonce_)) {
        boost::asio::post(
            get_executor(),
            std::bind(std::move(write_handler_),
                      boost::asio::error::operation_aborted, 0));
      }
      write_handler_ = {};

      read_nonce_ = {};
      write_nonce_ = {};
    }

   private:
    void HandleRequestRead(const base::error_code& ec,
                           io::SizeCallback callback,
                           std::shared_ptr<ReadContext> ctx) {
      base::FailIf(ec);

      // If we have been canceled here, just keep going because we
      // still expect to hear from our previous write.

      // Now we need to try and read the response.
      parent_->frame_stream_.AsyncRead(
          &parent_->rx_frame_,
          kDefaultTimeout,
          [self=shared_from_this(), callback = std::move(callback), ctx](
              const auto& ec) mutable {
            self->HandleRead(ec, std::move(callback), ctx);
          });
    }

    void HandleRead(const base::error_code& ec,
                    io::SizeCallback callback,
                    std::shared_ptr<ReadContext> ctx) {
      if (ctx->canceled) {
        callback(boost::asio::error::operation_aborted, 0u);
        return;
      }

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
          ctx->buffers,
          boost::asio::buffer(&frame.payload[stream.offset()],
                              *maybe_size));

      ctx->bytes_read += *maybe_size;

      // See if there is more waiting to be flushed out, in which
      // case, get it all before relinquishing to our final callback.
      if (parent_->frame_stream_.read_data_queued() &&
          *maybe_size < boost::asio::buffer_size(ctx->buffers)) {
        // Hmmm, we have more data available and room to put it.
        // This isn't possible unless either a slave was talking in
        // an unsolicited manner, or we actually got a response to a
        // previous request that had timed out, and our response is
        // still somewhere in the queue.  In any event, we'll want
        // to keep reading this to flush any data still on the
        // stream.

        auto offset_buffers =
            io::OffsetBufferSequence(ctx->buffers, *maybe_size);

        ctx->buffers = std::move(offset_buffers);

        HandleRequestRead(base::error_code(),
                          std::move(callback),
                          ctx);
        return;
      }

      callback({}, ctx->bytes_read);
    }

    void MakeFrame(io::ConstBufferSequence buffers, size_t size,
                   bool request_reply) {
      base::FastOStringStream stream;
      WriteStream writer{stream};

      writer.WriteVaruint(u32(Format::Subframe::kClientToServer));
      writer.WriteVaruint(channel_);

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
                    std::shared_ptr<ReadContext> ctx) {
      if (ctx->canceled) {
        boost::asio::post(
            parent_->executor_,
            std::bind(std::move(ctx->handler),
                      boost::asio::error::operation_aborted, 0));
        return;
      }

      base::FailIf(ec);

      if (ctx->bytes_read > 0) {
        // No need to retry, we're done.
        boost::asio::post(
            parent_->executor_,
            std::bind(std::move(ctx->handler),
                      base::error_code(), ctx->bytes_read));
        read_context_ = {};
        return;
      }

      // Yep, we failed to get anything this time.  Wait our polling
      // period and try again.
      poll_timer_.expires_from_now(options_.poll_rate);
      poll_timer_.async_wait(
          [self = shared_from_this(), ctx](auto&& ec) {
            if (ctx->canceled) {
              boost::asio::post(
                  self->get_executor(),
                  std::bind(std::move(ctx->handler),
                            boost::asio::error::operation_aborted, 0));
              return;
            }
            base::FailIf(ec);

            auto copy = std::move(ctx->handler);
            auto buffers = std::move(ctx->buffers);
            self->read_context_ = {};
            self->async_read_some(std::move(buffers), std::move(copy));
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
    std::shared_ptr<ReadContext> read_context_;
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
      return impl_->async_read_some(buffers, std::move(handler));
    }

    void async_write_some(io::ConstBufferSequence buffers,
                          io::WriteHandler handler) override {
      return impl_->async_write_some(buffers, std::move(handler));
    }

    boost::asio::any_io_executor get_executor() override {
      return impl_->get_executor();
    }

    void cancel() override {
      impl_->cancel();
    }

   private:
    std::shared_ptr<Tunnel> impl_;
  };

  const Options options_;
  FrameStream& frame_stream_;
  const FrameStream::Properties stream_properties_{frame_stream_.properties()};
  boost::asio::any_io_executor executor_{frame_stream_.get_executor()};

  io::ExclusiveCommand lock_{executor_};

  Frame rx_frame_;
  Frame tx_frame_;
  std::vector<Frame> tx_frames_;
  std::vector<const Frame*> tx_frame_ptrs_;
  std::vector<RegisterValue> parsed_values_;
};

StreamAsioClient::StreamAsioClient(FrameStream* stream, const Options& options)
    : impl_(std::make_unique<Impl>(stream, options)) {}

StreamAsioClient::~StreamAsioClient() {}

void StreamAsioClient::AsyncTransmit(
    const Request* request,
    Reply* reply,
    io::ErrorCallback handler) {
  impl_->AsyncTransmit(request, reply, std::move(handler));
}

io::SharedStream StreamAsioClient::MakeTunnel(
    uint8_t id, uint32_t channel, const TunnelOptions& options) {
  return impl_->MakeTunnel(id, channel, options);
}

}
}
