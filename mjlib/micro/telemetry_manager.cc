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

#include "mjlib/micro/telemetry_manager.h"

#include <cstdlib>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/stream.h"
#include "mjlib/base/tokenizer.h"

#include "mjlib/telemetry/format.h"

#include "mjlib/micro/pool_map.h"

namespace mjlib {
namespace micro {

namespace {
// Rates requested faster than this will result in a message being
// emitted per-update.
constexpr int kMinRateMs = 10;

// TODO(jpieper): Expose this as a constructor time option.
constexpr size_t kMaxElements = 16;
}

class TelemetryManager::Impl {
 public:
  struct Element {
    std::string_view name;
    int rate = 0;
    int next = 1;
    bool to_send = false;
    bool text = false;
    SerializableHandlerBase* base = nullptr;
  };

  using ElementPool = PoolMap<std::string_view, Element>;

  Impl(Pool* pool, CommandManager* command_manager,
       AsyncExclusive<AsyncWriteStream>* write_stream)
      : pool_(pool),
        write_stream_(write_stream),
        elements_(pool, kMaxElements) {
    command_manager->Register("tel", [this](auto&& name, auto&& response) {
        this->Command(name, response);
      });
  }

  void Command(const std::string_view& command,
               const CommandManager::Response& response) {
    base::Tokenizer tokenizer(command, " ");
    auto cmd = tokenizer.next();
    if (cmd == "get") {
      Get(tokenizer.remaining(), response);
    } else if (cmd == "list") {
      List(response);
    } else if (cmd == "schema") {
      Schema(tokenizer.remaining(), response);
    } else if (cmd == "rate") {
      Rate(tokenizer.remaining(), response);
    } else if (cmd == "fmt") {
      Format(tokenizer.remaining(), response);
    } else if (cmd == "stop") {
      Stop(response);
    } else if (cmd == "text") {
      Text(response);
    } else {
      UnknownCommand(cmd, response);
    }
  }

  void UpdateItem(ElementPool::iterator element_it) {
    // If we are in the mode where we emit on updates, mark this guy
    // as ready to update.  If we're not locked, then try to start
    // sending it out now.
    if (element_it->second.rate == 1) {
      element_it->second.to_send = true;
      MaybeStartSend();
    }
  }

  void PollMillisecond() {
    // Iterate over all of our elements, updating their next counters.
    for (auto& item_pair : elements_) {
      auto& element = item_pair.second;
      if (element.rate > 1 && element.next > 0) {
        element.next--;
        if (element.next == 0) {
          element.to_send = true;
          element.next = element.rate;
        }
      }
    }
    MaybeStartSend();
  }

  void MaybeStartSend() {
    if (outstanding_write_) { return; }

    // Look to see if something is good to send.
    //
    // We start checking one after the last sent item, that way we
    // fairly service all the channels.
    for (size_t i = 0; i < elements_.size(); i++) {
      const size_t to_check = (i + last_sent_ + 1) % elements_.size();
      auto& element = elements_[to_check].second;

      if (element.to_send) {
        element.to_send = false;
        outstanding_write_ = true;

        write_stream_->AsyncStart(
            [this, eptr = &element]
            (AsyncWriteStream* stream, VoidCallback release) {
              this->write_release_ = release;
              ErrorCallback actual_release = [this](error_code) {
                // TODO(jpieper): When we have logging or something,
                // report the error from here.
                this->outstanding_write_ = false;
                auto copy = this->write_release_;
                this->write_release_ = {};
                copy();
              };
              CommandManager::Response response{stream, actual_release};
              this->EmitData(eptr, response);
            });
        last_sent_ = to_check;
        return;
      }
    }
  }

  void Get(const std::string_view& name,
           const CommandManager::Response& response) {
    const auto it = elements_.find(name);
    if (it == elements_.end()) {
      WriteMessage(std::string_view("ERR unknown name\r\n"), response);
      return;
    }

    EmitData(&it->second, response);
  }

  void Enumerate(Element* element,
                 const CommandManager::Response& response) {
    current_response_ = response;

    element->base->Enumerate(
        &enumerate_context_,
        send_buffer_,
        element->name,
        *response.stream,
        [this](error_code) {
          this->WriteOK(current_response_);
        });
  }

  void EmitData(Element* element,
                const CommandManager::Response& response) {
    if (element->text) {
      Enumerate(element, response);
    } else {
      Emit(
          "emit ",
          element,
          [](Element* element, base::WriteStream* stream) {
            element->base->WriteBinary(*stream);
          },
          response);
    }
  }

  typedef base::inplace_function<void (Element*,
                                       base::WriteStream*)> WorkFunction;
  void Emit(const std::string_view& prefix,
            Element* element,
            WorkFunction work,
            const CommandManager::Response& response) {
    base::BufferWriteStream ostream{send_buffer_};
    ostream.write(prefix);
    ostream.write(element->name);
    ostream.write("\r\n");

    char* const size_position = send_buffer_ + ostream.offset();
    ostream.skip(sizeof(uint32_t));

    work(element, &ostream);

    base::BufferWriteStream size_stream({size_position, sizeof(uint32_t)});
    mjlib::telemetry::WriteStream tstream(size_stream);
    tstream.Write(static_cast<uint32_t>(
                      ostream.offset() + send_buffer_ -
                      (size_position + sizeof(uint32_t))));

    AsyncWrite(
        *response.stream,
        std::string_view(send_buffer_, ostream.offset()),
        response.callback);
  }

  void List(const CommandManager::Response& response) {
    current_response_ = response;
    current_list_index_ = 0;
    ListCallback({});
  }

  void ListCallback(error_code error) {
    if (error) {
      current_response_.callback(error);
      return;
    }

    if (current_list_index_ >= elements_.size()) {
      WriteOK(current_response_);
      return;
    }

    auto element_it = elements_.begin() + current_list_index_;
    if (element_it == elements_.end()) {
      WriteOK(current_response_);
      return;
    }

    auto& element = element_it->second;
    current_list_index_++;

    char *ptr = &send_buffer_[0];
    std::copy(element.name.begin(), element.name.end(), ptr);
    ptr += element.name.size();
    *ptr = '\r';
    ptr++;
    *ptr = '\n';
    ptr++;
    AsyncWrite(*current_response_.stream,
               std::string_view(send_buffer_, ptr - send_buffer_),
               [this](error_code ec) {
                 ListCallback(ec);
               });
  }

  void Schema(const std::string_view& name,
              const CommandManager::Response& response) {
    const auto element_it = elements_.find(name);
    if (element_it == elements_.end()) {
      WriteMessage("ERR unknown name\r\n", response);
      return;
    }

    Emit(
        "schema ",
        &element_it->second,
        [](Element* element, base::WriteStream* ostream) {
          element->base->WriteSchema(*ostream);
        },
        response);
  }

  void Rate(const std::string_view& command,
            const CommandManager::Response& response) {
    base::Tokenizer tokenizer(command, " ");
    auto name = tokenizer.next();
    auto rate_str = tokenizer.next();

    const auto element_it = elements_.find(name);
    if (element_it == elements_.end()) {
      WriteMessage("ERR unknown name\r\n", response);
      return;
    }

    auto& element = element_it->second;

    char buffer[24] = {};
    MJ_ASSERT(rate_str.size() < (sizeof(buffer) - 1));
    std::copy(rate_str.begin(), rate_str.end(), buffer);
    long rate = strtol(buffer, nullptr, 0);
    element.rate = rate;
    if (rate == 0) {
      element.next = 1;
    } else if (rate < kMinRateMs) {
      element.rate = 1;
      element.next = 1;
    } else {
      element.next = rate;
    }

    WriteOK(response);
  }

  void Format(const std::string_view& command,
              const CommandManager::Response& response) {
    base::Tokenizer tokenizer(command, " ");
    auto name = tokenizer.next();
    auto format_str = tokenizer.next();

    const auto element_it = elements_.find(name);
    if (element_it == elements_.end()) {
      WriteMessage("ERR unknown name\r\n", response);
      return;
    }

    auto& element = element_it->second;

    char buffer[5] = {};
    MJ_ASSERT(format_str.size() < (sizeof(buffer) - 1));
    std::copy(format_str.begin(), format_str.end(), buffer);
    const long format = strtol(buffer, nullptr, 0);
    element.text = format != 0;

    WriteOK(response);
  }

  void Stop(const CommandManager::Response& response) {
    for (auto& item_pair : elements_) {
      auto& element = item_pair.second;

      element.to_send = false;
      element.rate = 0;
    }

    WriteOK(response);
  }

  void Text(const CommandManager::Response& response) {
    for (auto& item_pair : elements_) {
      auto& element = item_pair.second;
      element.text = true;
    }

    WriteOK(response);
  }

  void UnknownCommand(const std::string_view&,
                      const CommandManager::Response& response) {
    WriteMessage("ERR unknown subcommand\r\n", response);
  }

  void WriteOK(const CommandManager::Response& response) {
    WriteMessage("OK\r\n", response);
  }

  void WriteMessage(const std::string_view& message,
                    const CommandManager::Response& response) {
    AsyncWrite(*response.stream, message, response.callback);
  }

  Pool* const pool_;
  AsyncExclusive<AsyncWriteStream>* const write_stream_;

  PoolMap<std::string_view, Element> elements_;

  CommandManager::Response current_response_;
  char send_buffer_[2048] = {};
  std::size_t current_list_index_ = 0;
  detail::EnumerateArchive::Context enumerate_context_;

  bool outstanding_write_ = false;
  VoidCallback write_release_;
  size_t last_sent_ = 0;
};

TelemetryManager::TelemetryManager(
    Pool* pool, CommandManager* command_manager,
    AsyncExclusive<AsyncWriteStream>* write_stream)
    : impl_(pool, pool, command_manager, write_stream) {}

TelemetryManager::~TelemetryManager() {}

void TelemetryManager::PollMillisecond() {
  impl_->PollMillisecond();
}

Pool* TelemetryManager::pool() const { return impl_->pool_; }

base::inplace_function<void ()> TelemetryManager::RegisterDetail(
    const std::string_view& name, SerializableHandlerBase* base) {
  Impl::Element element;
  element.name = name;
  element.base = base;

  const auto result = impl_->elements_.insert({name, element});
  // We don't allow duplicates.
  MJ_ASSERT(result.second == true);

  return [i = impl_.get(), element=result.first]() {
    i->UpdateItem(element);
  };
}

}
}
