// Demonstrates exception leakage from StreamFactory::AsyncCreate when
// a kSerial stream is requested with an invalid serial_parity value.
//
// Expected (async contract): handler is invoked with a non-empty error.
// Actual: make_parity() throws base::system_error::einval, which
// propagates out of AsyncCreateSerial synchronously.

#include "mjlib/io/stream_factory.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/system_error.h"

namespace {
struct PtyPair {
  int master = -1;
  std::string slave_path;

  PtyPair() {
    master = ::posix_openpt(O_RDWR | O_NOCTTY);
    BOOST_REQUIRE(master >= 0);
    BOOST_REQUIRE(::grantpt(master) == 0);
    BOOST_REQUIRE(::unlockpt(master) == 0);
    slave_path = ::ptsname(master);
  }

  ~PtyPair() {
    if (master >= 0) ::close(master);
  }
};
}

BOOST_AUTO_TEST_CASE(SerialFactoryInvalidParityDeliversErrorToHandler) {
  PtyPair pty;

  boost::asio::io_context context;
  mjlib::io::StreamFactory factory{context.get_executor()};

  mjlib::io::StreamFactory::Options options;
  options.type = mjlib::io::StreamFactory::Type::kSerial;
  options.serial_port = pty.slave_path;
  options.serial_low_latency = false;  // avoid TIOCSSERIAL on the pty
  options.serial_parity = "bogus";

  bool handler_called = false;
  mjlib::base::error_code received_ec;
  bool caught_exception = false;

  try {
    factory.AsyncCreate(
        options,
        [&](const mjlib::base::error_code& ec, mjlib::io::SharedStream) {
          handler_called = true;
          received_ec = ec;
        });
    context.run();
  } catch (const std::exception&) {
    caught_exception = true;
  }

  BOOST_TEST(!caught_exception);
  BOOST_TEST(handler_called);
  BOOST_TEST(static_cast<bool>(received_ec));
}
