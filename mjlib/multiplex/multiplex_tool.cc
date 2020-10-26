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

#include <optional>
#include <vector>

#include <clipp/clipp.h>

#include <fmt/format.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>

#include "mjlib/base/clipp.h"
#include "mjlib/base/clipp_archive.h"
#include "mjlib/base/error_code.h"
#include "mjlib/base/fail.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/io/deadline_timer.h"
#include "mjlib/io/stream_copy.h"
#include "mjlib/io/stream_factory.h"
#include "mjlib/multiplex/multiplex_tool.h"
#include "mjlib/multiplex/stream_asio_client_builder.h"

namespace mp = mjlib::multiplex;
namespace pl = std::placeholders;

namespace mjlib {
namespace multiplex {

namespace {
constexpr char kDelimiters[] = "\r\n;";

struct Options {
  std::vector<int> targets;

  bool console = false;
  bool register_tool = false;
  int poll_rate_ms = mp::AsioClient::TunnelOptions().poll_rate.total_milliseconds();
};

struct ValueFormatter {
  std::string operator()(int8_t value) {
    return fmt::format("{}b", value);
  }

  std::string operator()(int16_t value) {
    return fmt::format("{}s", value);
  }

  std::string operator()(int32_t value) {
    return fmt::format("{}i", value);
  }

  std::string operator()(float value) {
    return fmt::format("{}f", value);
  }
};

std::vector<std::string> Split(std::string_view str, std::string delimiter) {
  std::vector<std::string> result;
  boost::split(result, str, boost::is_any_of(delimiter));
  return result;
}

class CommandRunner {
 public:
  CommandRunner(const boost::asio::any_io_executor& executor,
                io::StreamFactory* stream_factory,
                io::Selector<AsioClient>* client_selector,
                const Options& options)
      : executor_(executor),
        client_selector_(client_selector),
        options_(options) {
    client_selector_->AsyncStart([this](const base::error_code& ec) {
        base::FailIf(ec);
        client_ = client_selector_->selected();
        MaybeStart();
      });

    io::StreamFactory::Options stdio_options;
    stdio_options.type = io::StreamFactory::Type::kStdio;

    stream_factory->AsyncCreate(
        stdio_options,
        std::bind(&CommandRunner::HandleConsole, this, pl::_1, pl::_2));
  }

  void HandleConsole(const base::error_code& ec, io::SharedStream stdio) {
    base::FailIf(ec);

    stdio_ = stdio;

    MaybeStart();
  }

  void MaybeStart() {
    if (!client_) { return; }
    if (!stdio_) { return; }

    RunCommand();
  }

  void RunCommand() {
    if (options_.console) {
      RunConsole();
    } else if (options_.register_tool) {
      RunRegister();
    } else {
      std::cerr << "no commands given!\n";
      std::exit(1);
    }
  }

  void RunConsole() {
    // TODO(jpieper): Auto-discover if empty.
    BOOST_ASSERT(!options_.targets.empty());

    mp::AsioClient::TunnelOptions tunnel_options;
    tunnel_options.poll_rate = boost::posix_time::milliseconds(options_.poll_rate_ms);
    tunnel_ = client_->MakeTunnel(options_.targets.at(0), 1, tunnel_options);
    copy_.emplace(executor_, tunnel_.get(), stdio_.get(),
                  std::bind(&CommandRunner::HandleDone, this, pl::_1));
  }

  void HandleDone(const mjlib::base::error_code& ec) {
    if (ec == boost::asio::error::eof) {
      std::exit(1);
    }
    mjlib::base::FailIf(ec);
  }

  void RunRegister() {
    StartRegisterRead();
  }

  void StartRegisterRead() {
    boost::asio::async_read_until(
        *stdio_,
        streambuf_,
        '\n',
        std::bind(&CommandRunner::HandleLine, this, pl::_1, pl::_2));
  }

  void HandleLine(const base::error_code& ec, size_t) {
    base::FailIf(ec);

    // Do we have a newline or ';' in our streambuf_?
    auto buffers = streambuf_.data();
    using const_buffers_type = boost::asio::streambuf::const_buffers_type;
    using iterator = boost::asio::buffers_iterator<const_buffers_type>;
    const auto begin = iterator::begin(buffers);
    const auto end = iterator::end(buffers);
    const auto it = std::find_if(
        begin, end, [&](char c) {
          return std::strchr(kDelimiters, c) != nullptr;
        });

    if (it == end) {
      // No current newlines, go and read some more.
      StartRegisterRead();
      return;
    }

    // This will eventually invoke HandleLine when it completes.
    ProcessRegisterCommand();
  }

  void ProcessRegisterCommand() {
    auto command_line = GetCommand();
    if (command_line.empty()) {
      HandleLine({}, 0);
      return;
    }

    // Commands are of the form:
    //
    // ID [op,]...
    //
    //  (wb|ws|wi|wf) REG value value value
    //   - write one or more consecutive registers
    //   - b = int8_t, s = int16_t, i = int32_t, f = float
    //
    //  (rb|rs|ri|rf) REG [NUM]
    //   - read one or more consecutive registers
    //
    // OR
    //
    // :123
    //   - a delay in milliseconds
    requests_.clear();
    reply_ = {};
    std::vector<std::string> devices = Split(command_line, "|");

    for (const auto& command : devices) {
      requests_.push_back({});
      auto& request = requests_.back();

      std::istringstream istr(command);
      std::string id_str;
      istr >> id_str;

      if (id_str.at(0) == ':') {
        delay_timer_.expires_from_now(
            boost::posix_time::milliseconds(std::stoi(id_str.substr(1))));
        delay_timer_.async_wait(
            std::bind(&CommandRunner::HandleDelayTimer, this, pl::_1));
        return;
      }

      std::vector<std::string> operators = Split(command.substr(istr.tellg()), ",");

      request.id = std::stoi(id_str);

      for (const auto& op : operators) {
        std::istringstream op_str(op);
        std::string op_name;
        op_str >> op_name;
        if (op_name == "wb" ||
            op_name == "ws" ||
            op_name == "wi" ||
            op_name == "wf") {
          // We are going to write things.
          std::string reg_str;
          op_str >> reg_str;
          std::vector<std::string> value_strs;
          while (true) {
            std::string value_str;
            op_str >> value_str;
            if (!op_str) { break; }
            value_strs.push_back(value_str);
          }

          AddWriteRequest(&request.request, op_name, reg_str, value_strs);
        } else if (op_name == "rb" ||
                   op_name == "rs" ||
                   op_name == "ri" ||
                   op_name == "rf") {
          // We are going to read things.
          std::string reg_str;
          op_str >> reg_str;

          std::string maybe_reg_count;
          op_str >> maybe_reg_count;

          AddReadRequest(&request.request, op_name, reg_str, maybe_reg_count);
        } else {
          std::cerr << "Unknown op name: " << op_name << "\n";
        }
      }
    }

    // Now we make our request.
    client_->AsyncTransmit(
        &requests_,
        &reply_,
        std::bind(&CommandRunner::HandleRequest, this, pl::_1));
  }

  void HandleDelayTimer(const base::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      return;
    }

    base::FailIf(ec);
    HandleLine({}, 0);
  }

  std::string FormatValue(mp::Format::ReadResult result) {
    if (std::holds_alternative<uint32_t>(result)) {
      return fmt::format("err/{}", std::get<uint32_t>(result));
    } else {
      return std::visit(ValueFormatter(), std::get<mp::Format::Value>(result));
    }
  }

  void HandleRequest(const base::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
      std::cout << "timeout\n";
      HandleLine({}, 0u);
      return;
    }

    base::FailIf(ec);

    bool first = true;

    // Group our replies by ID.
    std::map<int, std::vector<RegisterValue>> groups;

    for (const auto& item : reply_) {
      groups[item.id].push_back(std::make_pair(item.reg, item.value));
    }

    for (const auto& pair : groups) {
      if (!first) {
        std::cout << " | ";
      }
      first = false;
      const auto id = pair.first;
      const auto& reply = pair.second;

      if (reply.size() != 0) {
        std::cout << static_cast<int>(id) << ": ";

        std::cout << "{";
        bool first = true;
        for (auto& pair : reply) {
          if (!first) {
            std::cout << ", ";
          } else {
            first = false;
          }
          std::cout << fmt::format(
              "{}:{}",
              static_cast<int>(pair.first), FormatValue(pair.second));
        }
        std::cout << "}";
      }
    }
    std::cout << "\n";

    // Look for more commands, or possibly read more.
    HandleLine({}, 0u);
  }

  std::string GetCommand() {
    std::istream istr(&streambuf_);

    std::string result;
    while (true) {
      const int val = istr.get();
      if (!istr) { base::Fail("unexpected lack of command"); }

      if (std::strchr(kDelimiters, val) != nullptr) {
        // We have a command.
        return result;
      }

      result.push_back(val);
    }
  }

  size_t GetTypeIndex(char c) {
    switch (c) {
      case 'b': return 0;
      case 's': return 1;
      case 'i': return 2;
      case 'f': return 3;
    }
    base::AssertNotReached();
  }

  mp::Format::Value ParseValue(const std::string& str, size_t type_index) {
    switch (type_index) {
      case 0:
        return static_cast<int8_t>(std::stoi(str));
      case 1:
        return static_cast<int16_t>(std::stoi(str));
      case 2:
        return static_cast<int32_t>(std::stoi(str));
      case 3:
        return static_cast<float>(std::stod(str));
    }
    base::AssertNotReached();
  }

  void AddWriteRequest(mp::RegisterRequest* request,
                       const std::string& op_name,
                       const std::string& reg_str,
                       const std::vector<std::string>& value_strs) {
    const size_t type_index = GetTypeIndex(op_name.at(1));

    if (value_strs.size() == 1) {
      request->WriteSingle(std::stoi(reg_str),
                           ParseValue(value_strs.at(0), type_index));
    } else {
      std::vector<mp::Format::Value> values;
      for (const auto& value_str : value_strs) {
        values.push_back(ParseValue(value_str, type_index));
      }
      request->WriteMultiple(std::stoi(reg_str), values);
    }
  }

  void AddReadRequest(mp::RegisterRequest* request,
                      const std::string& op_name,
                      const std::string& reg_str,
                      const std::string& maybe_reg_count) {
    const auto type_index = GetTypeIndex(op_name.at(1));

    if (maybe_reg_count.empty()) {
      request->ReadSingle(std::stoi(reg_str), type_index);
    } else {
      request->ReadMultiple(std::stoi(reg_str), std::stoi(maybe_reg_count),
                            type_index);
    }
  }

  boost::asio::any_io_executor executor_;
  io::Selector<AsioClient>* const client_selector_;
  const Options options_;
  mp::AsioClient* client_ = nullptr;

  io::SharedStream tunnel_;
  io::SharedStream stdio_;
  std::optional<io::BidirectionalStreamCopy> copy_;

  boost::asio::streambuf streambuf_;
  mp::AsioClient::Request requests_;
  mp::AsioClient::Reply reply_;

  io::DeadlineTimer delay_timer_{executor_};
};
}

int multiplex_main(boost::asio::io_context& context,
                   int argc, char** argv,
                   io::Selector<AsioClient>* selector) {
  io::StreamFactory factory{context.get_executor()};

  io::Selector<AsioClient> default_frame_selector{
    context.get_executor(), "client_type"};
  if (selector == nullptr) {
    default_frame_selector.Register<StreamAsioClientBuilder>("stream");
    default_frame_selector.set_default("stream");
    selector = &default_frame_selector;
  }

  Options options;

  auto group = clipp::group(
      clipp::repeatable(
          (clipp::option("t", "target") &
           clipp::integer("TGT", options.targets)) % "one or more target devices"),
      clipp::option("c", "console").set(options.console),
      clipp::option("r", "register").set(options.register_tool),
      clipp::option("", "poll-rate-ms") & clipp::integer("MS", options.poll_rate_ms)
  );
  group.merge(clipp::with_prefix("client.", selector->program_options()));

  mjlib::base::ClippParse(argc, argv, group);

  CommandRunner command_runner{
    context.get_executor(), &factory, selector, options};

  context.run();

  return 0;
}

}
}
