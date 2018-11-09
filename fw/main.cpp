// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include <inttypes.h>

#include "mbed.h"
#include "mbed_events.h"
#include "rtos_idle.h"

#include "async_stream.h"
#include "stm32f446_async_uart.h"
#include "stm32f446_bldc_foc.h"

namespace {

static constexpr char kMessage[] = "hello\r\n";

class Emitter {
 public:
  Emitter(EventQueue* queue, AsyncStream* stream, DigitalOut* led, Stm32F446BldcFoc* bldc)
      : stream_(stream), led_(led), bldc_(bldc) {
    queue->call_every(1000, this, &Emitter::Emit);

    StartRead();
  }

  void Emit() {
    *led_ = !(*led_);

    const auto& status = bldc_->status();
    printf("Emit %" PRIu32 " %d  %d %d\r\n", count_++, printing_,
           status.adc1_raw, status.adc2_raw);

    if (printing_) { return; }
    printing_ = true;

    AsyncWrite(*stream_, string_view::ensure_z(kMessage), [this](ErrorCode ec) {
        MJ_ASSERT(ec == 0);
        printf("callback\r\n");
        printing_ = false;
      });
  }

  void StartRead() {
    stream_->AsyncReadSome(string_span(readbuf_, sizeof(readbuf_) - 1),
                           [this](ErrorCode ec, size_t amount) {
                             this->HandleRead(ec, amount);
                           });
  }

 private:
  void HandleRead(ErrorCode ec, size_t amount) {
    if (ec != 0) {
      printf("got err: %x\r\n", ec);
    }

    readbuf_[amount] = 0;
    const int int_amount = static_cast<int>(amount);
    printf("read %d bytes: '%s'\r\n", int_amount, readbuf_);

    StartRead();
  }

  AsyncStream* const stream_;
  DigitalOut* const led_;
  Stm32F446BldcFoc* const bldc_;

  bool printing_ = false;
  uint32_t count_ = 0;

  char readbuf_[16] = {};
};

void new_idle_loop() {
}
}

int main(void) {
  // We want no sleep modes at all for highest timing resolution
  // w.r.t. interrupts.
  rtos_attach_idle_hook(&new_idle_loop);

  EventQueue queue(4096);

  DigitalOut led(LED1);

  Stm32F446AsyncUart::Options pc_options;
  pc_options.tx = PC_10;
  pc_options.rx = PC_11;
  pc_options.baud_rate = 9600;
  Stm32F446AsyncUart pc(&queue, pc_options);

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

  Emitter emitter(&queue, &pc, &led, &bldc);

  queue.dispatch_forever();

  return 0;
}
