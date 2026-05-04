// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
//
// Reproducer for handler-throws-from-RealtimeExecutor bug.
//
// When a handler posted through RealtimeExecutor throws, the
// implementation fails to call StopWork() (and event_timer_.Stop()).
// This leaves outstanding_work_ stuck above zero so the idle_timer
// keeps running and ultimately fires SIGABRT even though the io_context
// has fully drained.
//
// We run the scenario inside a forked child with a 50ms idle timeout
// and wait for the child either to exit cleanly (bug absent) or to
// abort via the realtime timer (bug present).

#include "mjlib/io/realtime_executor.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/test/auto_unit_test.hpp>

namespace {

[[noreturn]] void RunChild() {
  boost::asio::io_context context;
  mjlib::io::RealtimeExecutor dut{context.get_executor()};
  mjlib::io::RealtimeExecutor::Options options;
  options.idle_timeout_ns = 50'000'000;  // 50 ms
  dut.set_options(options);

  boost::asio::any_io_executor executor{dut};
  boost::asio::post(executor, []() {
    throw std::runtime_error("intentional throw");
  });

  try {
    context.run();
  } catch (const std::exception&) {
    // Expected.
  }

  // The handler is fully drained.  If RealtimeExecutor were
  // exception-safe, the idle_timer would already have been stopped and
  // we should be able to sleep here without aborting.
  ::usleep(200'000);  // 200 ms
  std::cout << "child reached end without aborting" << std::endl;
  std::_Exit(0);
}

}  // namespace

BOOST_AUTO_TEST_CASE(RealtimeExecutorThrowingHandlerLeak) {
  pid_t pid = ::fork();
  BOOST_REQUIRE(pid >= 0);

  if (pid == 0) {
    RunChild();
  }

  int status = 0;
  BOOST_REQUIRE(::waitpid(pid, &status, 0) == pid);

  const bool aborted = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
  if (aborted) {
    BOOST_FAIL(
        "Child aborted via idle_timer after a handler threw -- "
        "RealtimeExecutor::Wrap is not exception safe");
  }
  BOOST_TEST(WIFEXITED(status));
  BOOST_TEST(WEXITSTATUS(status) == 0);
}
