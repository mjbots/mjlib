// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

#include <cstdint>

#include "opaque_ptr.h"

/// Commands a BLDC motor.  Pin and peripheral assignments are
/// hardcoded as: TBD
class Stm32F466BldcFoc {
 public:
  Stm32F466BldcFoc();
  ~Stm32F466BldcFoc();

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

 private:
  class Impl;
  OpaquePtr<Impl, 512> impl_;
};
