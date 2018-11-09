// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include <inttypes.h>

#include <functional>

#include "mbed.h"
#include "mbed_events.h"
#include "rtos_idle.h"

#include "async_stream.h"
#include "command_manager.h"
#include "stm32f446_async_uart.h"
#include "stm32f446_bldc_foc.h"

namespace {

static constexpr char kMessage[] = "hello\r\n";

class Emitter {
 public:
  Emitter(DigitalOut* led) : led_(led) {}

  void HandleCommand(const string_view& command, const CommandManager::Response& response) {
    if (command == string_view::ensure_z("on")) {
      *led_ = 1;
    } else if (command == string_view::ensure_z("off")) {
      *led_ = 0;
    } else {
      AsyncWrite(*response.stream, string_view::ensure_z("UNKNOWN\r\n"), response.callback);
      return;
    }
    AsyncWrite(*response.stream, string_view::ensure_z("OK\r\n"), response.callback);
  }

 private:
  DigitalOut* const led_;
};

void new_idle_loop() {
}
}

int main(void) {
  // We want no sleep modes at all for highest timing resolution
  // w.r.t. interrupts.
  rtos_attach_idle_hook(&new_idle_loop);

  EventQueue queue(4096);

  DigitalOut led(LED2);

  Stm32F446AsyncUart::Options pc_options;
  pc_options.tx = PC_10;
  pc_options.rx = PC_11;
  pc_options.baud_rate = 9600;
  Stm32F446AsyncUart pc(&queue, pc_options);

  AsyncExclusive<AsyncWriteStream> exclusive_write(&pc);
  CommandManager command_manager(&queue, &pc, &exclusive_write);

  Stm32F446BldcFoc::Options bldc_options;
  bldc_options.pwm1 = PA_0;
  bldc_options.pwm2 = PA_1;
  bldc_options.pwm3 = PA_2;

  bldc_options.current1 = PC_5;
  bldc_options.current2 = PB_0_ALT0;
  bldc_options.vsense = PC_1_ALT1;

  bldc_options.debug_out = PB_3;

  Stm32F446BldcFoc bldc{bldc_options};
  Stm32F446BldcFoc::CommandData bldc_command;
  bldc_command.mode = Stm32F446BldcFoc::kPhasePwm;
  bldc_command.phase_a_millipercent = 2000;
  bldc_command.phase_b_millipercent = 3000;
  bldc_command.phase_c_millipercent = 4000;

  bldc.Command(bldc_command);

  Emitter emitter(&led);

  command_manager.Register(string_view::ensure_z("led"),
                           std::bind(&Emitter::HandleCommand, &emitter,
                                     std::placeholders::_1, std::placeholders::_2));

  command_manager.AsyncStart();
  queue.dispatch_forever();

  return 0;
}
