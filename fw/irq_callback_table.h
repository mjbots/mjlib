// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include "static_function.h"

/// A helper class to bind arbitrary callbacks into IRQ handlers.
class IrqCallbackTable {
 public:
  using IrqFunction = void (*)();

  /// A RAII class that deregisters the callback when destroyed.  A
  /// default constructed instance is valid and has a null
  /// irq_function.
  class Callback {
   public:
    Callback(IrqFunction in = nullptr) : irq_function(in) {}
    ~Callback();

    Callback(const Callback&) = delete;
    Callback& operator=(const Callback&) = delete;

    Callback(Callback&& rhs) : irq_function(rhs.irq_function) {
      rhs.irq_function = nullptr;
    }

    Callback& operator=(Callback&& rhs) {
      irq_function = rhs.irq_function;
      rhs.irq_function = nullptr;
      return *this;
    }

    IrqFunction irq_function = nullptr;
  };

  /// Given an arbitrary callback, return a function pointer suitable
  /// for use as an interrupt handler.  When invoked, the given
  /// callback will be called.
  static Callback MakeFunction(StaticFunction<void()> callback);
};
