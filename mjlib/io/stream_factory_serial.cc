// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/io/stream_factory_serial.h"

#ifndef _WIN32
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <fcntl.h>
#endif  // _WIN32

#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/serial_port.hpp>

#include "mjlib/base/system_error.h"

namespace mjlib {
namespace io {
namespace detail {

namespace {
class SerialStream : public AsyncStream {
 public:
  SerialStream(const boost::asio::any_io_executor& executor,
               const StreamFactory::Options& options)
      : executor_(executor),
        options_(options),
        port_(executor) {}

  void Open(mjlib::base::error_code* ec) {
    boost::system::error_code boost_ec;
    port_.open(options_.serial_port, boost_ec);
    if (boost_ec) {
      *ec = boost_ec;
      return;
    }

    BOOST_ASSERT(options_.type == StreamFactory::Type::kSerial);
    port_.set_option(
        boost::asio::serial_port_base::baud_rate(options_.serial_baud));
    port_.set_option(
        boost::asio::serial_port_base::character_size(options_.serial_data_bits));

#ifndef _WIN32
    {
      struct serial_struct serial;
      ioctl(port_.native_handle(), TIOCGSERIAL, &serial);
      if (options_.serial_low_latency) {
        serial.flags |= ASYNC_LOW_LATENCY;
      } else {
        serial.flags &= ~ASYNC_LOW_LATENCY;
      }
      ioctl(port_.native_handle(), TIOCSSERIAL, &serial);
    }
#else  // _WIN32
    {
      if (options_.serial_low_latency) {
        COMMTIMEOUTS new_timeouts = {MAXDWORD, 0, 0, 0, 0};
        SetCommTimeouts(port_.native_handle(), &new_timeouts);
      }
    }
#endif  // _WIN32

    auto make_parity = [](std::string string) {
      boost::to_lower(string);
      if (string == "n" || string == "none") {
        return boost::asio::serial_port_base::parity::none;
      } else if (string == "e" || string == "even") {
        return boost::asio::serial_port_base::parity::even;
      } else if (string == "o" || string == "odd") {
        return boost::asio::serial_port_base::parity::odd;
      }
      throw base::system_error::einval("unknown parity: " + string);
    };
    port_.set_option(
        boost::asio::serial_port_base::parity(make_parity(options_.serial_parity)));
  }

  ~SerialStream() override {}

  boost::asio::any_io_executor get_executor() override { return executor_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    port_.async_read_some(buffers, std::move(handler));
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    port_.async_write_some(buffers, std::move(handler));
  }

  void cancel() override {
    port_.cancel();
  }

 private:
  boost::asio::any_io_executor executor_;
  const StreamFactory::Options options_;
  boost::asio::serial_port port_;
};
}

void AsyncCreateSerial(
    const boost::asio::any_io_executor& executor,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  auto stream = std::make_shared<SerialStream>(executor, options);
  base::error_code ec;
  stream->Open(&ec);

  if (ec) {
    ec.Append("When opening: '" + options.serial_port + "'");
  }

  boost::asio::post(executor, std::bind(std::move(handler), ec, stream));
}

}
}
}
