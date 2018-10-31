// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#include "stm32f466_bldc_foc.h"

#include "mbed.h"
#include "PeripheralPins.h"

#include "irq_callback_table.h"

// TODO
//
//  * Enable/disable the DRV8323
//  * Implement current controllers

// mbed seems to configure the TIM1 clock input to 180MHz.  We want
// 40kHz update rate, so:
//
//  180000000 / 40000 = 4500
constexpr uint32_t kPwmCounts = 4500;

class Stm32F466BldcFoc::Impl {
 public:
  Impl() {
    MBED_ASSERT(!g_impl_);
    g_impl_ = this;

    // Hardware assignment:
    //
    // TIM1: Primary PWM generation.
    //  * Center-aligned PWM (up/down counting)
    //  * 40kHz frequency (for 20kHz PWM)
    //
    // PA_8, PA_9, PA_10 configured as PWM outputs.
    //
    // ADC1/2/3
    //  * PC_0, PC_1, PA_0 as analog inputs

    ConfigureADC();
    ConfigureTim1();
  }

  ~Impl() {
    g_impl_ = nullptr;
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

    // NOTE: We don't use IrqCallbackTable here because we need the
    // absolute minimum latency possible.
    NVIC_SetVector(TIM1_UP_TIM10_IRQn, reinterpret_cast<uint32_t>(&Impl::GlobalInterrupt));
    NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 2);
    NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

    // Reinitialize the counter and update all registers.
    tim1_->EGR |= TIM_EGR_UG;

    // Finally, enable TIM1.
    tim1_->CR1 |= TIM_CR1_CEN;
  }

  void ConfigureADC() {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    // Triple mode: Regular simultaneous mode only.
    ADC->CCR = (0x16 << ADC_CCR_MULTI_Pos);

    // Turn on all the converters.
    ADC1->CR2 = ADC_CR2_ADON;
    ADC2->CR2 = ADC_CR2_ADON;
    ADC3->CR2 = ADC_CR2_ADON;

    // We rely on the AnalogIn members to configure the pins as
    // inputs.

    // Set sample times to 15 cycles across the board
    ADC1->SMPR1 = 0x01;  // PC_0 is channel 0 for ADC1
    ADC2->SMPR1 = 0x08;  // PC_1 is channel 1 for ADC2
    ADC3->SMPR2 = 0x01;  // PA_0 is channel 0 for ADC3
  }

  // CALLED IN INTERRUPT CONTEXT.
  static void GlobalInterrupt() {
    g_impl_->HandleTimer();
  }

  // CALLED IN INTERRUPT CONTEXT.
  void HandleTimer() {
    debug_out_ = 1;

    if (tim1_->SR & TIM_SR_UIF) {
      // Start conversion.
      ADC1->CR2 |= ADC_CR2_SWSTART;

      while ((ADC1->SR & ADC_SR_EOC) == 0);

      status_.adc1_raw = ADC1->DR;
      status_.adc2_raw = ADC2->DR;
      status_.adc3_raw = ADC3->DR;

      status_.cur1_A = status_.adc1_raw * config_.i_scale_A;
      status_.cur2_A = status_.adc2_raw * config_.i_scale_A;
      status_.bus_V = status_.adc3_raw * config_.v_scale_V;
    }

    // Reset the status register.
    tim1_->SR = 0x00;

    debug_out_ = 0;
  }

  const Config config_;
  TIM_TypeDef* const tim1_ = TIM1;
  ADC_TypeDef* const adc1_ = ADC1;

  // We create these to initialize our pins as output and PWM mode,
  // but otherwise don't use them.
  PwmOut pa8_{PA_8};
  PwmOut pa9_{PA_9};
  PwmOut pa10_{PA_10};

  AnalogIn current1_{PC_0};
  AnalogIn current2_{PC_1};
  AnalogIn current3_{PA_0};

  // This is just for debugging.
  DigitalOut debug_out_{PB_3};

  CommandData data_;

  Status status_;

  static Impl* g_impl_;
};

Stm32F466BldcFoc::Impl* Stm32F466BldcFoc::Impl::g_impl_ = nullptr;

Stm32F466BldcFoc::Stm32F466BldcFoc() : impl_() {}
Stm32F466BldcFoc::~Stm32F466BldcFoc() {}

void Stm32F466BldcFoc::Command(const CommandData& data) {
  impl_->Command(data);
}
