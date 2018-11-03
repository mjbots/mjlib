// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include <cstdint>

#include "PinNames.h"

#include "opaque_ptr.h"

/// Commands a BLDC motor.  Pin and peripheral assignments are
/// hardcoded as: TBD
class Stm32F466BldcFoc {
 public:
  struct Options {
    // These three pins must be on the same timer, and one that
    // supports center aligned PWM.
    PinName pwm1 = NC;
    PinName pwm2 = NC;
    PinName pwm3 = NC;

    PinName current1 = NC;  // Must be sample-able from ADC1
    PinName current2 = NC;  // Must be sample-able from ADC2

    PinName current3 = NC;
    PinName vsense = NC;
    PinName vtemp = NC;

    PinName debug_out = NC;
  };

  Stm32F466BldcFoc(const Options&);
  ~Stm32F466BldcFoc();

  struct Config {
    float i_scale_A = 0.02014f;  // Amps per A/D LSB
    float v_scale_V = 0.00884f;  // V per A/D count
  };

  struct Status {
    uint16_t adc1_raw = 0;
    uint16_t adc2_raw = 0;
    uint16_t adc3_raw = 0;

    float cur1_A = 0.0;
    float cur2_A = 0.0;
    float bus_V = 0.0;
  };

  enum Mode {
    kDisabled,
    kPhasePwm,
    kFoc,
  };

  struct CommandData {
    Mode mode = kDisabled;

    // For kPhasePwm mode.
    uint16_t phase_a_millipercent = 0;  // 0 - 10000
    uint16_t phase_b_millipercent = 0;  // 0 - 10000
    uint16_t phase_c_millipercent = 0;  // 0 - 10000

    // For kFoc mode.
    int32_t i_d_mA = 0;
    int32_t i_q_mA = 0;
  };

  void Command(const CommandData&);

  Status status() const;

 private:
  class Impl;
  OpaquePtr<Impl, 512> impl_;
};
