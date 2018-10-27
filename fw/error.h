// Copyright 2018 Josh Pieper, jjp@pobox.com.  All rights reserved.

#pragma once

constexpr int kDmaStreamTransferError = 0x201;
constexpr int kDmaStreamFifoError = 0x202;

constexpr int kUartOverrunError = 0x300;
constexpr int kUartFramingError = 0x301;
constexpr int kUartNoiseError = 0x302;
constexpr int kUartBufferOverrunError = 0x303;
