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

#include "mjlib/multiplex/threaded_client.h"

#include <sched.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <future>
#include <optional>
#include <sstream>
#include <thread>

#include <boost/crc.hpp>

#include <fmt/format.h>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/system_error.h"
#include "mjlib/multiplex/frame.h"
#include "mjlib/multiplex/stream.h"

namespace mjlib {
namespace multiplex {

namespace {
void ThrowIf(bool value, std::string_view message = "") {
  if (value) {
    throw base::system_error::syserrno(message.data());
  }
}

std::string Hexify(const std::string_view& data) {
  std::ostringstream ostr;
  for (auto c : data) {
    ostr << fmt::format("{:02x}", static_cast<int>(c));
  }
  return ostr.str();
}

class SystemFd {
 public:
  SystemFd(int fd) : fd_(fd) {}
  SystemFd() {}

  ~SystemFd() {
    if (fd_ >= 0) { ::close(fd_); }
  }

  SystemFd(const SystemFd&) = delete;
  SystemFd& operator=(const SystemFd&) = delete;

  SystemFd(SystemFd&& rhs) {
    fd_ = rhs.fd_;
    rhs.fd_ = -1;
  }

  SystemFd& operator=(SystemFd&& rhs) {
    fd_ = rhs.fd_;
    rhs.fd_ = -1;
    return *this;
  }

  operator int() const { return fd_; }

 private:
  int fd_;
};

struct FrameItem {
  Frame frame;
  char encoded[256] = {};
  std::size_t size = 0;
};
}

class ThreadedClient::Impl {
 public:
  Impl(boost::asio::io_service& service,
       const Options& options)
      : service_(service),
        options_(options) {
    Start();
  }

  ~Impl() {
    child_service_.stop();
    thread_.join();
  }

  void Start() {
    thread_ = std::thread(std::bind(&Impl::Run, this));
    startup_promise_.set_value(true);
  }

  void AsyncRegister(const Request* request,
                     Reply* reply,
                     io::ErrorCallback callback) {
    AssertParent();
    child_service_.post(std::bind(&Impl::ThreadAsyncRegister,
                                  this, request, reply, callback));
  }

  Stats stats() const {
    Stats result;
    result.checksum_errors = checksum_errors_.load();
    result.timeouts = timeouts_.load();
    result.malformed = malformed_.load();
    result.extra_found = extra_found_.load();
    return result;
  }

 private:
  void Run() {
    startup_future_.get();
    AssertThread();

    if (options_.cpu_affinity >= 0) {
      cpu_set_t cpuset = {};
      CPU_ZERO(&cpuset);
      CPU_SET(options_.cpu_affinity, &cpuset);

      ThrowIf(::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0);
    }

    // TODO: Set realtime priority and lock memory.

    OpenPort();

    select_timeout_.tv_sec = 0;
    select_timeout_.tv_nsec = options_.query_timeout_s * 1000000000;

    boost::asio::io_service::work work(child_service_);
    child_service_.run();
  }

  void OpenPort() {
    AssertThread();

    if (options_.fd >= 0) {
      fd_ = options_.fd;
    } else {
      fd_ = OpenHardwareSerialPort(options_.port, options_.baud_rate);
    }
  }

  void ThreadAsyncRegister(const Request* requests,
                           Reply* reply,
                           io::ErrorCallback callback) {
    AssertThread();

    MJ_ASSERT(!requests->requests.empty());

    FrameItem* prev_frame = nullptr;
    FrameItem* this_frame = &frame_item1_;
    FrameItem* next_frame = &frame_item2_;

    auto encode_frame = [&](FrameItem* frame_item,
                            const SingleRequest& next_request) {
      // Now we encode the next frame.
      frame_item->frame.source_id = 0;
      frame_item->frame.dest_id = next_request.id;
      frame_item->frame.request_reply = next_request.request.request_reply();
      frame_item->frame.payload = next_request.request.buffer();
      base::BufferWriteStream stream{{frame_item->encoded,
              sizeof(frame_item->encoded)}};
      frame_item->frame.encode(&stream);
      frame_item->size = stream.offset();
    };

    encode_frame(this_frame, requests->requests.front());

    // We stick all the processing work we can right after sending a
    // request.  That way it overlaps with the time it takes for the
    // device to respond.
    for (size_t i = 0; i < requests->requests.size(); i++) {
      ThrowIf(::write(fd_, this_frame->encoded, this_frame->size) !=
              static_cast<ssize_t>(this_frame->size));

      if (i + 1 < requests->requests.size()) {
        encode_frame(next_frame, requests->requests[i + 1]);
      }

      // Now we would parse the prior frame.
      if (prev_frame && prev_frame->frame.request_reply) {
        ParseFrame(prev_frame, reply);
      }

      if (this_frame->frame.request_reply) {
        // And now we read this one.  Re-purpose our 'buf' to be for
        // reading from writing earlier.
        this_frame->size = 0;
        ReadFrame(this_frame);
      }

      // Now lets cycle all our frame pointers to be ready for the
      // next round.
      auto* new_next = prev_frame ? prev_frame : &frame_item3_;
      prev_frame = this_frame;
      this_frame = next_frame;
      next_frame = new_next;
    }

    // Parse the final frame.
    if (prev_frame->frame.request_reply) {
      ParseFrame(prev_frame, reply);
    }

    // Now we can report success.
    service_.post(std::bind(callback, base::error_code()));
  }

  void ParseFrame(const FrameItem* frame_item, Reply* reply) {
    base::BufferReadStream buffer_stream(
        {frame_item->encoded, frame_item->size});
    ReadStream<base::BufferReadStream> stream{buffer_stream};

    stream.Read<uint16_t>();  // ignore header
    const auto maybe_source = stream.Read<uint8_t>();
    const auto maybe_dest = stream.Read<uint8_t>();
    const auto packet_size = stream.ReadVaruint();

    if (!maybe_source || !maybe_dest || !packet_size) {
      malformed_++;
      return;
    }

    SingleReply this_reply;
    this_reply.id = *maybe_source;

    base::BufferReadStream payload_stream{
      {buffer_stream.position(), *packet_size}};
    this_reply.reply = ParseRegisterReply(payload_stream);

    buffer_stream.ignore(*packet_size);
    const auto maybe_read_crc = stream.Read<uint16_t>();
    if (!maybe_read_crc) {
      malformed_++;
      return;
    }
    const auto read_crc = *maybe_read_crc;
    boost::crc_ccitt_type crc;
    crc.process_bytes(frame_item->encoded, frame_item->size - 2);
    const auto expected_crc = crc.checksum();

    if (read_crc != expected_crc) {
      checksum_errors_++;

      if (options_.debug_checksum_errors) {
        std::cout << "csum_error: "
                  << Hexify({frame_item->encoded, frame_item->size}) << "\n";
      }
      return;
    }

    reply->replies.push_back(std::move(this_reply));
  }

  struct Packet {
    const char* start = nullptr;
    std::size_t size = 0;

    Packet(const char* start_in, std::size_t size_in)
        : start(start_in),
          size(size_in) {}
  };

  std::optional<Packet> FindCompletePacket() {
    auto size = receive_pos_;
    auto* const packet = static_cast<const char*>(
        ::memmem(receive_buffer_, size, &multiplex::Format::kHeader, 2));
    if (packet == nullptr) { return {}; }

    const auto offset = packet - receive_buffer_;
    size -= offset;

    if (size < 7) {
      // This is below the minimum size.
      return {};
    }

    base::BufferReadStream base_stream(
        {packet + 4, static_cast<std::size_t>(size - 4)});
    multiplex::ReadStream<base::BufferReadStream> stream(base_stream);

    const auto packet_size = stream.ReadVaruint();
    if (!packet_size) {
      return {};
    }
    if (*packet_size > (size - 6)) {
      return {};
    }

    // OK, we've got a full packet worth.
    return Packet{packet, static_cast<std::size_t>(
          4 + base_stream.offset() + *packet_size + 2)};
  }

  void ReadFrame(FrameItem* frame_item) {
    while (true) {
      {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(static_cast<int>(fd_), &read_fds);
        const int result = ::pselect(fd_ + 1, &read_fds, nullptr, nullptr,
                                     &select_timeout_, nullptr);
        if (result < 0 && errno == EINTR) {
          // Try again.
          continue;
        }
        ThrowIf(result < 0);
        if (result == 0) {
          // A timeout.
          timeouts_++;
          return;
        }
      }
      const int result = ::read(fd_, receive_buffer_ + receive_pos_,
                                sizeof(receive_buffer_) - receive_pos_);
      if (result < 0 && errno == EINTR) {
        // Try again.
        continue;
      }
      ThrowIf(result < 0);

      if (result == 0) {
        // Timeout.  We'll just return an empty buffer to indicate
        // that.
        timeouts_++;
        return;
      }

      receive_pos_ += result;

      const auto maybe_found = FindCompletePacket();
      if (maybe_found) {
        const auto found = *maybe_found;
        std::memcpy(frame_item->encoded, found.start, found.size);

        if (receive_pos_ != static_cast<std::streamsize>(
                (found.start - receive_buffer_) + found.size)) {
          // Whoops, we have more data than we asked for.  A previous
          // request must have timed out and we are receiving it now.
          // Lets just discard it all for now. :(
          extra_found_++;
        }

        frame_item->size = found.size;

        receive_pos_ = 0;

        return;
      }
    }
  }

  static SystemFd OpenHardwareSerialPort(const std::string& name, int baud) {
    SystemFd fd{::open(name.c_str(), O_RDWR | O_CLOEXEC | O_NOCTTY)};
    ThrowIf(fd < 0, "opening: " + name);

    {
      struct termios options = {};
      ThrowIf(::tcgetattr(fd, &options) < 0);

      MJ_ASSERT(baud == 3000000);
      const auto baud = B3000000;

      options.c_cflag = baud | CS8 | CLOCAL | CREAD;

      // Set up timeouts: Calls to read() are basically non-blocking.
      options.c_cc[VTIME] = 0;
      options.c_cc[VMIN] = 0;

      ThrowIf(::tcflush(fd, TCIOFLUSH) < 0);
      ThrowIf(::tcsetattr(fd, TCSANOW, &options) < 0);
    }

    {
      struct serial_struct serial;
      ThrowIf(::ioctl(fd, TIOCGSERIAL, &serial) < 0);
      serial.flags |= ASYNC_LOW_LATENCY;
      ThrowIf(::ioctl(fd, TIOCSSERIAL, &serial) < 0);
    }

    return fd;
  }

  void AssertThread() {
    MJ_ASSERT(std::this_thread::get_id() == thread_.get_id());
  }

  void AssertParent() {
    MJ_ASSERT(std::this_thread::get_id() != thread_.get_id());
  }

  boost::asio::io_service& service_;
  const Options options_;

  std::thread thread_;

  // Accessed from both.
  std::promise<bool> startup_promise_;
  std::future<bool> startup_future_ = startup_promise_.get_future();

  std::atomic<uint64_t> checksum_errors_{0};
  std::atomic<uint64_t> timeouts_{0};
  std::atomic<uint64_t> malformed_{0};
  std::atomic<uint64_t> extra_found_{0};

  // Only accessed from the child thread.
  boost::asio::io_service child_service_;
  SystemFd fd_;

  FrameItem frame_item1_;
  FrameItem frame_item2_;
  FrameItem frame_item3_;

  char receive_buffer_[256] = {};
  std::streamsize receive_pos_ = 0;

  struct timespec select_timeout_ = {};
};

ThreadedClient::ThreadedClient(boost::asio::io_service& service,
                               const Options& options)
    : impl_(std::make_unique<Impl>(service, options)) {}

ThreadedClient::~ThreadedClient() {}

void ThreadedClient::AsyncRegister(const Request* request,
                                   Reply* reply,
                                   io::ErrorCallback callback) {
  impl_->AsyncRegister(request, reply, callback);
}

ThreadedClient::Stats ThreadedClient::stats() const {
  return impl_->stats();
}

}
}
