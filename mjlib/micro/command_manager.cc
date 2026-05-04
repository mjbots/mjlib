// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/micro/command_manager.h"

#include "mjlib/base/string_span.h"
#include "mjlib/base/tokenizer.h"

#include "mjlib/micro/async_read.h"
#include "mjlib/micro/pool_map.h"

namespace mjlib {
namespace micro {

class CommandManager::Impl {
 public:
  Impl(Pool* pool,
       AsyncReadStream* read_stream,
       AsyncExclusive<AsyncWriteStream>* write_stream,
       const Options& options)
      : read_stream_(read_stream),
        write_stream_(write_stream),
        options_(options),
        registry_(pool, 16),
        line_buffer_(reinterpret_cast<char*>(
                         pool->Allocate(options.max_line_length, 1))),
        arguments_(reinterpret_cast<char*>(
                       pool->Allocate(options.max_line_length, 1))),
        streambuf_data_(reinterpret_cast<char*>(
                            pool->Allocate(options.max_line_length, 1))),
        read_streambuf_({streambuf_data_, options.max_line_length}, 0) {}

  void MaybeStartRead() {
    if (write_outstanding_) { return; }
    // Idempotent against being called twice while a read is already
    // in flight. A command handler that completes synchronously
    // (e.g. by calling its response.callback inline) reaches
    // MaybeStartRead from within the response-completion path AND
    // again from the trailing call at the end of HandleRead; without
    // this guard the second call issues a concurrent AsyncReadSome
    // and trips the underlying stream's single-outstanding-read
    // invariant.
    if (read_outstanding_) { return; }

    read_until_context_.stream = read_stream_;
    read_until_context_.streambuf = &read_streambuf_;
    read_until_context_.result =
        base::string_span(line_buffer_, options_.max_line_length);
    read_until_context_.delimiters = "\r\n";
    read_until_context_.callback =
        [this](error_code error, int size) {
      this->read_outstanding_ = false;
      this->HandleRead(error, size);
    };

    read_outstanding_ = true;
    AsyncReadUntil(read_until_context_);
  }

  void HandleRead(error_code error, int size) {
    if (error) {
      // Well, not much we can do, but ignore everything until we get
      // another newline and try again.

      // TODO jpieper: Once we have an error system, log this error.

      read_until_context_.stream = read_stream_;
      read_until_context_.streambuf = &read_streambuf_;
      read_until_context_.result =
          base::string_span(line_buffer_, options_.max_line_length);
      read_until_context_.delimiters = "\r\n";
      read_until_context_.callback = [this](error_code, int) {
        this->read_outstanding_ = false;
        this->MaybeStartRead();
      };
      read_outstanding_ = true;
      AsyncIgnoreUntil(read_until_context_);
      return;
    }

    HandleCommand(size);

    MaybeStartRead();
  }

  void HandleCommand(int size) {
    MJ_ASSERT(!write_outstanding_);

    // Make our command, minus whatever the delimeter was that ended
    // it.
    const std::string_view line(line_buffer_, size - 1);

    base::Tokenizer tokenizer(line, " ");
    auto cmd = tokenizer.next();
    if (cmd.size() == 0) { return; }

    const auto it = registry_.find(cmd);

    CommandFunction command;
    if (it == registry_.end()) {
      command = [this](const std::string_view&, const Response& response) {
        this->UnknownGroup(response);
      };
    } else {
      command = it->second.command_function;
    }

    current_command_ = command;
    auto args = tokenizer.remaining();

    // Clear out anything that was previously in our arguments, then
    // fill it in with our new stuff.
    std::memset(arguments_, 0, options_.max_line_length);
    std::memcpy(arguments_, args.data(), args.size());
    group_arguments_ = base::string_span(arguments_, args.size());

    // We're done with line_buffer_ now, so clear it out to make
    // debugging easier.
    std::memset(line_buffer_, 0, options_.max_line_length);

    write_outstanding_ = true;
    write_stream_->AsyncStart(
        [this](AsyncWriteStream* actual_write_stream, VoidCallback done_callback) {
          auto callback = this->current_command_;
          this->current_command_ = CommandFunction();
          auto args = std::string_view(this->group_arguments_.data(),
                                       this->group_arguments_.size());
          this->group_arguments_ = base::string_span();

          this->done_callback_ = done_callback;

          Response context{actual_write_stream,
                [this](error_code) {
              this->write_outstanding_ = false;
              auto done = this->done_callback_;
              this->done_callback_ = {};
              done();

              this->MaybeStartRead();
            }
          };
          callback(args, context);
        });
  }

  void UnknownGroup(const Response& response) {
    AsyncWrite(*response.stream,
               std::string_view("ERR unknown command\r\n"),
               response.callback);
  }

  struct Item {
    CommandFunction command_function;
  };

  AsyncReadStream* const read_stream_;
  AsyncExclusive<AsyncWriteStream>* const write_stream_;
  const Options options_;

  using Registry = PoolMap<std::string_view, Item>;
  Registry registry_;
  bool write_outstanding_ = false;
  bool read_outstanding_ = false;

  char* const line_buffer_;
  char* const arguments_;
  char* const streambuf_data_;

  base::string_span group_arguments_;
  CommandFunction current_command_;
  VoidCallback done_callback_;

  AsyncReadUntilContext read_until_context_;

  AsyncStreamBuf read_streambuf_;
};

CommandManager::CommandManager(
    Pool* pool,
    AsyncReadStream* read_stream,
    AsyncExclusive<AsyncWriteStream>* write_stream,
    const Options& options)
    : impl_(pool, pool, read_stream, write_stream, options) {}

CommandManager::~CommandManager() {}

void CommandManager::Register(const std::string_view& name,
                              CommandFunction command_function) {
  const auto result = impl_->registry_.insert(
      {name, Impl::Item{command_function}});
  // We do not allow duplicate names.
  MJ_ASSERT(result.second == true);
}

void CommandManager::AsyncStart() {
  impl_->MaybeStartRead();
}

}
}
