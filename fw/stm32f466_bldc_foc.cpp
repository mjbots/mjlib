// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include "stm32f466_bldc_foc.h"

#include "mbed.h"
#include "PeripheralPins.h"

#include "irq_callback_table.h"

// TODO
//
//  * Output debugging events via digital outs
//  * Enable/disable the DRV8323
//  * Sample currents
//  * Implement current controllers
//

// mbed seems to configure the TIM1 clock input to 180MHz.  We want
// 40kHz update rate, so:
//
//  180000000 / 40000 = 4500
constexpr uint32_t kPwmCounts = 4500;

class Stm32F466BldcFoc::Impl {
 public:
  Impl()
      : pa8_(PA_8),
        pa9_(PA_9),
        pa10_(PA_10) {
    // Hardware assignment:
    //
    // TIM1: Primary PWM generation.
    //  * Center-aligned PWM (up/down counting)
    //  * 40kHz frequency (for 20kHz PWM)
    //
    // PA_8, PA_9, PA_10 configured as PWM outputs.

    // ADC?

    ConfigureTim1();
  }

  void Command(const CommandData& data) {
    data_ = data;

    switch (data_.mode) {
      case kDisabled: {
        // TODO(jpieper)
        break;
      }
      case kPhasePwm: {
        tim1_->CCR1 = static_cast<uint32_t>(data.phase_a_millipercent) * kPwmCounts / 10000;
        tim1_->CCR2 = static_cast<uint32_t>(data.phase_b_millipercent) * kPwmCounts / 10000;
        tim1_->CCR3 = static_cast<uint32_t>(data.phase_c_millipercent) * kPwmCounts / 10000;
        break;
      }
      case kFoc: {
        break;
      }
    }
  }

 private:
  void ConfigureTim1() {
    __HAL_RCC_TIM1_CLK_ENABLE();

    // Enable the update interrupt.
    tim1_->DIER = TIM_DIER_UIE;

    // Enable the update interrupt.
    tim1_->CR1 =
        // Center-aligned mode 2.  The counter counts up and down
        // alternatively.  Output compare interrupt flags of channels
        // configured in output are set only when the counter is
        // counting up.
        (2 << TIM_CR1_CMS_Pos) |

        // ARR register is buffered.
        TIM_CR1_ARPE;

    // Update once per up/down of the counter.
    tim1_->RCR |= 0x01;

    // Set up PWM.

    tim1_->PSC = 0; // No prescaler.
    tim1_->ARR = kPwmCounts;

    // Configure the first three outputs with positive polarity.
    tim1_->CCER = // |= ~(TIM_CCER_CC1NP);
        TIM_CCER_CC1E | TIM_CCER_CC1P |
        TIM_CCER_CC2E | TIM_CCER_CC2P |
        TIM_CCER_CC3E | TIM_CCER_CC3P;

    timer_callback_ = IrqCallbackTable::MakeFunction([this]() { this->HandleTimer(); });
    NVIC_SetVector(TIM1_UP_TIM10_IRQn, reinterpret_cast<uint32_t>(timer_callback_.irq_function));
    NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

    // Reinitialize the counter and update all registers.
    tim1_->EGR |= TIM_EGR_UG;

    // Finally, enable TIM1.
    tim1_->CR1 |= TIM_CR1_CEN;
  }

  // CALLED IN INTERRUPT CONTEXT.
  void HandleTimer() {
    if (tim1_->SR & TIM_SR_UIF) {
    }

    // Reset the status register.
    tim1_->SR = 0x00;
  }

  TIM_TypeDef* const tim1_ = TIM1;

  // We create these to initialize our pins as output and PWM mode,
  // but otherwise don't use them.
  PwmOut pa8_;
  PwmOut pa9_;
  PwmOut pa10_;

  CommandData data_;
  IrqCallbackTable::Callback timer_callback_;
};

Stm32F466BldcFoc::Stm32F466BldcFoc() : impl_() {}
Stm32F466BldcFoc::~Stm32F466BldcFoc() {}

void Stm32F466BldcFoc::Command(const CommandData& data) {
  impl_->Command(data);
}
