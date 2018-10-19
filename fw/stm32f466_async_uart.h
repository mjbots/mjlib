// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include "EventQueue.h"
#include "PinNames.h"

#include "async_stream.h"
#include "opaque_ptr.h"

/// Presents a single USART on the STM32F466 as an AsyncStream.
class Stm32F466AsyncUart : public AsyncStream {
 public:
  struct Options {
    PinName tx = NC;
    PinName rx = NC;
    int baud_rate = 115200;
  };

  /// @param event_queue - All callbacks will be invoked from this, it
  /// is aliased internally and must live as long as the instance.
  Stm32F466AsyncUart(events::EventQueue* event_queue, const Options&);
  ~Stm32F466AsyncUart() override;

  void AsyncReadSome(const string_span&, const SizeCallback&) override;
  void AsyncWriteSome(const string_view&, const SizeCallback&) override;

 private:
  class Impl;
  OpaquePtr<Impl, 384> impl_;
};
