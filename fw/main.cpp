// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include <inttypes.h>

#include "mbed.h"
#include "mbed_events.h"

#include "async_stream.h"
#include "stm32f466_async_uart.h"

namespace {

static constexpr char kMessage[] = "hello\r\n";

class Emitter {
 public:
  Emitter(EventQueue* queue, AsyncStream* stream, DigitalOut* led)
      : stream_(stream), led_(led) {
    queue->call_every(1000, this, &Emitter::Emit);

    StartRead();
  }

  void Emit() {
    *led_ = !(*led_);

    printf("Emit %" PRIu32 " %d\r\n", count_++, printing_);

    if (printing_) { return; }
    printing_ = true;

    AsyncWrite(*stream_, string_view::ensure_z(kMessage), [this](ErrorCode ec) {
        MBED_ASSERT(ec == 0);
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

  bool printing_ = false;
  uint32_t count_ = 0;

  char readbuf_[16] = {};
};
}

int main(void) {
  EventQueue queue(4096);

  DigitalOut led(LED1);

  Stm32F466AsyncUart::Options pc_options;
  pc_options.tx = PC_10;
  pc_options.rx = PC_11;
  pc_options.baud_rate = 9600;
  Stm32F466AsyncUart pc(&queue, pc_options);

  Emitter emitter(&queue, &pc, &led);

  queue.dispatch_forever();

  return 0;
}
