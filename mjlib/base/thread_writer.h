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

#pragma once

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <string_view>
#include <thread>

#include <boost/circular_buffer.hpp>
#include <boost/noncopyable.hpp>

#include "mjlib/base/fail.h"
#include "mjlib/base/fast_stream.h"
#include "mjlib/base/system_error.h"
#include "mjlib/base/system_fd.h"
#include "mjlib/base/system_file.h"

namespace mjlib {
namespace base {

/// Write data to a file in a background thread, so that the API can
/// be safe to use from asynchronous functions.  (It can also be
/// configured in blocking mode).
class ThreadWriter : boost::noncopyable {
 public:
  class OStream : public FastOStringStream {
   public:
    ~OStream() override {}
    OStream() {}

    void set_start(size_t start) { start_ = start; }
    size_t start() const { return start_; }

    void clear() {
      data()->clear();
      start_ = 0;
    }

    size_t size() const { return data()->size() - start_; }

    size_t start_ = 0;
  };

  using Buffer = std::unique_ptr<OStream>;

  class Reclaimer {
   public:
    virtual void Reclaim(Buffer) = 0;
  };

  enum BlockingMode {
    kAsynchronous,
    kBlocking,
  };

  struct Options {
    BlockingMode blocking_mode = kBlocking;
    ssize_t block_size = 1 << 20;
    int rt_signal = SIGRTMIN + 10;
    double flush_timeout_s = 1.0;
    Reclaimer* reclaimer = nullptr;

    Options() {}
  };

  /// @param realtime - if 'true', then a hard error will occur if
  /// data cannot be written to disk fast enough.  If 'false', the API
  /// call will simply block.
  ThreadWriter(std::string_view filename, const Options& options = {})
      : ThreadWriter(OpenName(filename), options) {}

  ThreadWriter(int fd, const Options& options = {})
      : ThreadWriter(OpenFd(fd), options) {}

  ThreadWriter(FILE* fd, const Options& options = {})
      : options_(options),
        parent_id_(std::this_thread::get_id()),
        fd_(fd),
        pipe_(MakePipe()),
        thread_(std::bind(&ThreadWriter::Run, this)) {
    BOOST_ASSERT(fd != nullptr);
  }

  ~ThreadWriter() {
    BOOST_ASSERT(std::this_thread::get_id() == parent_id_);
    {
      std::lock_guard<std::mutex> lock(command_mutex_);
      done_ = true;
    }
    SignalThread();

    thread_.join();
    WriteAll();
  }

  void Flush() {
    BOOST_ASSERT(std::this_thread::get_id() == parent_id_);
    {
      std::lock_guard<std::mutex> lock(command_mutex_);
      flush_ = true;
    }
    SignalThread();
  }

  void Write(std::unique_ptr<OStream> buffer) {
    BOOST_ASSERT(std::this_thread::get_id() == parent_id_);
    position_ += buffer->size();
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      if (data_.full()) { data_.set_capacity(data_.capacity() * 2); }
      data_.push_back(std::move(buffer));
    }
    SignalThread();
  }

  void DoTimer() {
    // This will be invoked from a signal handler, so it could be in a
    // random thread.
    timer_fired_.store(true);
    SignalThread();
  }

  uint64_t position() const {
    BOOST_ASSERT(std::this_thread::get_id() == parent_id_);
    return position_;
  }

 private:
  struct Pipe {
    SystemFd read;
    SystemFd write;
  };


  static FILE* OpenName(std::string_view name) {
    FILE* result = ::fopen(name.data(), "wb");
    system_error::throw_if(result == nullptr,
                           "While opening: " + std::string(name));
    return result;
  }

  static Pipe MakePipe() {
    int fds[2] = {};
    mjlib::base::FailIfErrno(::pipe(fds) < 0);

    // We make the writing side non-blocking.
    system_error::throw_if(::fcntl(fds[1], F_SETFL, O_NONBLOCK) < 0);

    Pipe result{fds[0], fds[1]};
    return result;
  }

  static FILE* OpenFd(int fd) {
    FILE* result = ::fdopen(fd, "wb");
    mjlib::base::FailIfErrno(result == nullptr);
    return result;
  }

  void SignalThread() {
    // This can be called from the parent thread, or a signal handler
    // (which could be in a random thread.

    char c = 0;
    while (true) {
      int err = ::write(pipe_.write, &c, 1);
      if (err == 1) { return; }
      if (errno == EAGAIN ||
          errno == EWOULDBLOCK) {
        // Yikes, I guess we are backed up.  Just return, because
        // apparently the receiver is plenty well signaled.
        return;
      }
      if (errno != EINTR) {
        // Hmmm, we might be inside of a POSIX signal handler.  Just
        // abort.
        ::raise(SIGABRT);
      }
    }
  }

  void Run() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());

    ::setvbuf(fd_, buf_, _IOFBF, sizeof(buf_));

    if (options_.blocking_mode == kAsynchronous) {
      StartTimer();
    }

    while (true) {
      char c = {};
      int err = ::read(pipe_.read, &c, 1);
      if (err < 0 && errno != EINTR) {
        mjlib::base::FailIfErrno(true);
      }

      {
        std::lock_guard<std::mutex> lock(command_mutex_);
        if (done_) {
          RunWork();
          HandleFlush();
          break;
        }
        if (flush_) {
          HandleFlush();
        }
      }

      if (timer_fired_) {
        HandleTimer();
      }

      RunWork();
    }

    if (options_.blocking_mode == kAsynchronous) {
      StopTimer();
    }
  }

  void RunWork() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());

    while (true) {
      {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (data_.empty()) { return; }
      }

      // No one can remove things from data but us, so we don't need
      // to check again.
      StartWrite();
    }
  }

  static void timer_handler(int, siginfo_t* si, void *) {
    auto writer = static_cast<ThreadWriter*>(si->si_value.sival_ptr);
    writer->DoTimer();
  }

  void StartTimer() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());

    struct sigaction act;
    std::memset(&act, 0, sizeof(act));
    act.sa_sigaction = &ThreadWriter::timer_handler;
    act.sa_flags = SA_SIGINFO;
    mjlib::base::FailIfErrno(sigaction(options_.rt_signal, &act, nullptr) < 0);

    struct sigevent sevp;
    std::memset(&sevp, 0, sizeof(sevp));
    sevp.sigev_notify = SIGEV_SIGNAL;
    sevp.sigev_value.sival_ptr = this;
    sevp.sigev_signo = options_.rt_signal;

    mjlib::base::FailIfErrno(::timer_create(CLOCK_MONOTONIC, &sevp, &timer_id_) < 0);

    struct itimerspec value;
    std::memset(&value, 0, sizeof(value));
    value.it_interval.tv_sec = static_cast<int>(options_.flush_timeout_s);
    value.it_interval.tv_nsec =
        static_cast<long>((options_.flush_timeout_s -
                           value.it_interval.tv_sec) * 1e9);
    value.it_value = value.it_interval;
    mjlib::base::FailIfErrno(::timer_settime(timer_id_, 0, &value, nullptr) < 0);
  }

  void StopTimer() {
    mjlib::base::FailIfErrno(::timer_delete(timer_id_) < 0);
  }

  void HandleTimer() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());
    ::fflush(fd_);
  }

  void StartWrite() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());

    WriteFront();

    // At block boundaries, let the OS know that we don't plan on
    // using this data anytime soon so as to not fill up the page
    // cache.
    if (options_.blocking_mode == kAsynchronous) {
      if ((child_offset_ - last_fadvise_) > options_.block_size) {
        ::fflush(fd_);
        int64_t next_fadvise = last_fadvise_;
        while ((next_fadvise + options_.block_size) < child_offset_) {
          next_fadvise += options_.block_size;
        }
        ::posix_fadvise(fileno(fd_),
                        last_fadvise_,
                        next_fadvise - last_fadvise_,
                        POSIX_FADV_DONTNEED);
        last_fadvise_ = next_fadvise;
      }
    }
  }

  void WriteAll() {
    BOOST_ASSERT(std::this_thread::get_id() == parent_id_);
    while (!data_.empty()) {
      WriteFront();
    }
  }

  void HandleFlush() {
    BOOST_ASSERT(std::this_thread::get_id() == thread_.get_id());
    ::fflush(fd_);
  }

  void WriteFront() {
    // NOTE: This can only be called by the parent thread during the
    // final write-out, after the child thread has stopped.

    std::unique_ptr<OStream> buffer;
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      buffer = std::move(data_.front());
      data_.pop_front();
    }

    const OStream& stream = *buffer;

    const char* ptr = &(*stream.data())[stream.start()];
    size_t size = stream.size();
    size_t result = ::fwrite(ptr, size, 1, fd_);
    mjlib::base::FailIfErrno(result == 0);

    child_offset_ += size;
    if (options_.reclaimer) {
      options_.reclaimer->Reclaim(std::move(buffer));
    }
  }

  // Parent items.
  const Options options_;
  std::thread::id parent_id_;
  timer_t timer_id_ = {};
  uint64_t position_ = 0;

  // Initialized from parent, then only accessed from child.
  SystemFile fd_;
  const Pipe pipe_;

  // Only accessed from child thread.
  int64_t child_offset_ = 0;
  int64_t last_fadvise_ = 0;
  char buf_[65536] = {};

  // All threads.
  std::thread thread_;

  std::mutex data_mutex_;
  boost::circular_buffer<std::unique_ptr<OStream> > data_{16};

  std::atomic<bool> timer_fired_{false};

  // The following booleans are protected by the associated mutex.
  std::mutex command_mutex_;
  bool flush_ = false;
  bool done_ = false;
};

}
}
