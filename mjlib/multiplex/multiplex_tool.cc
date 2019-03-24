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

#include <boost/asio/io_service.hpp>
#include <boost/program_options.hpp>

#include "mjlib/base/error_code.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/program_options_archive.h"
#include "mjlib/io/async_stream.h"
#include "mjlib/io/stream_copy.h"
#include "mjlib/io/stream_factory.h"
#include "mjlib/multiplex/asio_client.h"

namespace base = mjlib::base;
namespace io = mjlib::io;
namespace mp = mjlib::multiplex;
namespace po = boost::program_options;
namespace pl = std::placeholders;

namespace {
struct Options {
  std::vector<int> targets;

  bool console = false;
};

class CommandRunner {
 public:
  CommandRunner(io::StreamFactory* stream_factory,
                const io::StreamFactory::Options& stream_options,
                const Options& options)
      : stream_factory_(stream_factory),
        options_(options) {
    stream_factory->AsyncCreate(
        stream_options,
        std::bind(&CommandRunner::HandleStream, this, pl::_1, pl::_2));
  }

  void HandleStream(const base::error_code& ec, io::SharedStream stream) {
    base::FailIf(ec);

    stream_ = stream;
    client_.emplace(stream_.get());

    RunCommand();
  }

  void RunCommand() {
    // TODO(jpieper): Auto-discover if empty.
    BOOST_ASSERT(!options_.targets.empty());

    RunSingleTarget(
        options_.targets.front(),
        std::vector<int>(std::next(options_.targets.begin()),
                         options_.targets.end()));
  }

  void RunSingleTarget(int target, const std::vector<int>&) {
    if (options_.console) {
      tunnel_ = client_->MakeTunnel(target, 1);

      io::StreamFactory::Options stdio_options;
      stdio_options.type = io::StreamFactory::Type::kStdio;

      stream_factory_->AsyncCreate(
          stdio_options,
          std::bind(&CommandRunner::HandleConsole, this, pl::_1, pl::_2));
    } else {
      std::cerr << "No commands given!\n";
      std::exit(1);
    }
  }

  void HandleConsole(const base::error_code& ec, io::SharedStream stdio) {
    base::FailIf(ec);

    stdio_ = stdio;
    copy_.emplace(tunnel_.get(), stdio_.get());

    // We can only have one console at a time, so we never need to
    // re-invoke RunSingleTarget.
  }

  io::StreamFactory* const stream_factory_;
  const Options options_;
  io::SharedStream stream_;
  std::optional<mp::AsioClient> client_;

  io::SharedStream tunnel_;
  io::SharedStream stdio_;
  std::optional<io::BidirectionalStreamCopy> copy_;
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
      ;
  base::ProgramOptionsArchive(&desc).Accept(&stream_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return 0;
  }

  CommandRunner command_runner{&factory, stream_options, options};

  service.run();

  return 0;
}
