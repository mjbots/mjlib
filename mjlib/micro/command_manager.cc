// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
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
#include "mjlib/micro/named_registry.h"

namespace mjlib {
namespace micro {

namespace {
constexpr size_t kMaxLineLength = 100;
}

class CommandManager::Impl {
 public:
  Impl(AsyncReadStream* read_stream,
       AsyncExclusive<AsyncWriteStream>* write_stream)
      : read_stream_(read_stream),
        write_stream_(write_stream) {
  }

  void StartRead() {
    read_until_context_.stream = read_stream_;
    read_until_context_.buffer = base::string_span(line_buffer_);
    read_until_context_.delimiters = "\r\n";
    read_until_context_.callback =
        [this](base::error_code error, int size) {
      this->HandleRead(error, size);
    };

    AsyncReadUntil(read_until_context_);
  }

  void HandleRead(base::error_code error, int size) {
    if (error) {
      // Well, not much we can do, but ignore everything until we get
      // another newline and try again.

      // TODO jpieper: Once we have an error system, log this error.

      read_until_context_.stream = read_stream_;
      read_until_context_.buffer = base::string_span(line_buffer_);
      read_until_context_.delimiters = "\r\n";
      read_until_context_.callback = [this](base::error_code, int) {
        this->StartRead();
      };
      AsyncIgnoreUntil(read_until_context_);
      return;
    }

    HandleCommand(size);

    StartRead();
  }

  void HandleCommand(int size) {
    // If we haven't managed to emit our last message yet, just ignore
    // this command entirely. :(
    if (write_outstanding_) { return; }

    // Make our command, minus whatever the delimeter was that ended
    // it.
    const std::string_view line(line_buffer_, size - 1);

    base::Tokenizer tokenizer(line, " ");
    auto cmd = tokenizer.next();
    if (cmd.size() == 0) { return; }

    auto* element = registry_.FindOrCreate(cmd, kFindOnly);

    CommandFunction command;
    if (element == nullptr) {
      command = [this](const std::string_view&, const Response& response) {
        this->UnknownGroup(response);
      };
    } else {
      command = element->command_function;
    }

    current_command_ = command;
    auto args = tokenizer.remaining();

    // Clear out anything that was previously in our arguments, then
    // fill it in with our new stuff.
    std::memset(arguments_, 0, sizeof(arguments_));
    std::memcpy(arguments_, args.data(), args.size());
    group_arguments_ = base::string_span(arguments_, args.size());

    // We're done with line_buffer_ now, so clear it out to make
    // debugging easier.
    std::memset(line_buffer_, 0, sizeof(line_buffer_));

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
                [this](base::error_code) {
              this->write_outstanding_ = false;
              auto done = this->done_callback_;
              this->done_callback_ = {};
              done();
            }
          };
          callback(args, context);
        });
  }

  void UnknownGroup(const Response& response) {
    AsyncWrite(*response.stream,
               std::string_view("unknown command\r\n"),
               response.callback);
  }

  struct Item {
    CommandFunction command_function;
  };

  AsyncReadStream* const read_stream_;
  AsyncExclusive<AsyncWriteStream>* const write_stream_;

  using Registry = NamedRegistry<Item, 16>;
  Registry registry_;
  bool write_outstanding_ = false;

  char line_buffer_[kMaxLineLength] = {};
  char arguments_[kMaxLineLength] = {};

  base::string_span group_arguments_;
  CommandFunction current_command_;
  VoidCallback done_callback_;

  AsyncReadUntilContext read_until_context_;
};

CommandManager::CommandManager(
    Pool* pool,
    AsyncReadStream* read_stream,
    AsyncExclusive<AsyncWriteStream>* write_stream)
    : impl_(pool, read_stream, write_stream) {}

CommandManager::~CommandManager() {}

void CommandManager::Register(const std::string_view& name,
                              CommandFunction command_function) {
  auto* item = impl_->registry_.FindOrCreate(name, kAllowCreate);
  item->command_function = command_function;
}

void CommandManager::AsyncStart() {
  impl_->StartRead();
}

}
}
