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

#include <optional>
#include <vector>

#include <fmt/format.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/program_options.hpp>

#include "mjlib/base/error_code.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/program_options_archive.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/io/deadline_timer.h"
#include "mjlib/io/stream_copy.h"
#include "mjlib/io/stream_factory.h"
#include "mjlib/multiplex/asio_client.h"

namespace base = mjlib::base;
namespace io = mjlib::io;
namespace mp = mjlib::multiplex;
namespace po = boost::program_options;
namespace pl = std::placeholders;

namespace {
constexpr char kDelimiters[] = "\r\n;";

struct Options {
  std::vector<int> targets;

  bool console = false;
  bool register_tool = false;
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
  CommandRunner(boost::asio::io_service& service,
                io::StreamFactory* stream_factory,
                const io::StreamFactory::Options& stream_options,
                const Options& options)
      : service_(service),
        stream_factory_(stream_factory),
        options_(options) {
    stream_factory->AsyncCreate(
        stream_options,
        std::bind(&CommandRunner::HandleStream, this, pl::_1, pl::_2));

    io::StreamFactory::Options stdio_options;
    stdio_options.type = io::StreamFactory::Type::kStdio;

    stream_factory_->AsyncCreate(
        stdio_options,
        std::bind(&CommandRunner::HandleConsole, this, pl::_1, pl::_2));
  }

  void HandleConsole(const base::error_code& ec, io::SharedStream stdio) {
    base::FailIf(ec);

    stdio_ = stdio;

    MaybeStart();
  }

  void HandleStream(const base::error_code& ec, io::SharedStream stream) {
    base::FailIf(ec);

    stream_ = stream;
    client_.emplace(stream_.get());

    MaybeStart();
  }

  void MaybeStart() {
    if (!stream_) { return; }
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

    tunnel_ = client_->MakeTunnel(options_.targets.at(0), 1);
    copy_.emplace(tunnel_.get(), stdio_.get());
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
    auto command = GetCommand();
    if (command.empty()) {
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

    request_ = {};

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

        AddWriteRequest(&request_, op_name, reg_str, value_strs);
      } else if (op_name == "rb" ||
                 op_name == "rs" ||
                 op_name == "ri" ||
                 op_name == "rf") {
        // We are going to read things.
        std::string reg_str;
        op_str >> reg_str;

        std::string maybe_reg_count;
        op_str >> maybe_reg_count;

        AddReadRequest(&request_, op_name, reg_str, maybe_reg_count);
      } else {
        std::cerr << "Unknown op name: " << op_name << "\n";
      }
    }

    // Now we make our request.
    const int id = std::stoi(id_str);
    client_->AsyncRegister(id, request_,
                           std::bind(&CommandRunner::HandleRequest, this,
                                     pl::_1, pl::_2, id));
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

  void HandleRequest(const base::error_code& ec, const mp::RegisterReply& reply,
                     int id) {
    if (ec == boost::asio::error::operation_aborted) {
      std::cout << "timeout\n";
      HandleLine({}, 0u);
      return;
    }

    base::FailIf(ec);

    if (reply.size() != 0) {
      std::cout << id << ": ";

      std::cout << "{";
      bool first = true;
      for (auto& pair : reply) {
        if (!first) {
          std::cout << ", ";
        } else {
          first = false;
        }
        std::cout << fmt::format("{}:{}", pair.first, FormatValue(pair.second));
      }
      std::cout << "}\n";
    }

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

  boost::asio::io_service& service_;
  io::StreamFactory* const stream_factory_;
  const Options options_;
  io::SharedStream stream_;
  std::optional<mp::AsioClient> client_;

  io::SharedStream tunnel_;
  io::SharedStream stdio_;
  std::optional<io::BidirectionalStreamCopy> copy_;

  boost::asio::streambuf streambuf_;
  mp::RegisterRequest request_;

  io::DeadlineTimer delay_timer_{service_};
};
}

int main(int argc, char** argv) {
  boost::asio::io_service service;
  io::StreamFactory factory{service};

  io::StreamFactory::Options stream_options;
  po::options_description desc("Allowable options");

  Options options;

  desc.add_options()
      ("help,h", "display usage message")
      ("target,t", po::value(&options.targets), "")
      ("console,c", po::bool_switch(&options.console), "")
      ("register,r", po::bool_switch(&options.register_tool), "")
      ;
  base::ProgramOptionsArchive(&desc).Accept(&stream_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return 0;
  }

  CommandRunner command_runner{service, &factory, stream_options, options};

  service.run();

  return 0;
}
