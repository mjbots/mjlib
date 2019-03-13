// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include <linux/serial.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <fcntl.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio/serial_port.hpp>

#include "mjlib/base/system_error.h"

namespace mjlib {
namespace io {
namespace detail {

namespace {
class SerialStream : public AsyncStream {
 public:
  SerialStream(boost::asio::io_service& service,
               const StreamFactory::Options& options)
      : service_(service),
        port_(service, options.serial_port) {
    BOOST_ASSERT(options.type == StreamFactory::Type::kSerial);
    port_.set_option(
        boost::asio::serial_port_base::baud_rate(options.serial_baud));
    port_.set_option(
        boost::asio::serial_port_base::character_size(options.serial_data_bits));

    {
      struct serial_struct serial;
      ioctl(port_.native_handle(), TIOCGSERIAL, &serial);
      if (options.serial_low_latency) {
        serial.flags |= ASYNC_LOW_LATENCY;
      } else {
        serial.flags &= ~ASYNC_LOW_LATENCY;
      }
      ioctl(port_.native_handle(), TIOCSSERIAL, &serial);
    }

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
        boost::asio::serial_port_base::parity(make_parity(options.serial_parity)));
  }

  ~SerialStream() override {}

  boost::asio::io_service& get_io_service() override { return service_; }

  void async_read_some(MutableBufferSequence buffers,
                       ReadHandler handler) override {
    port_.async_read_some(buffers, handler);
  }

  void async_write_some(ConstBufferSequence buffers,
                        WriteHandler handler) override {
    port_.async_write_some(buffers, handler);
  }

  void cancel() override {
    port_.cancel();
  }

 private:
  boost::asio::io_service& service_;
  boost::asio::serial_port port_;
};
}

void AsyncCreateSerial(
    boost::asio::io_service& service,
    const StreamFactory::Options& options,
    StreamHandler handler) {
  service.post(
      std::bind(handler, base::error_code(),
                std::make_shared<SerialStream>(service, options)));
}

}
}
}
