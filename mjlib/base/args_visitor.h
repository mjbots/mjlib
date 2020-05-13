// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "mjlib/base/visitor.h"

namespace mjlib {
namespace base {

struct ArgumentTraits {
  const char* help = nullptr;
  const char* label = nullptr;
};

template <typename T>
class ReferenceNameValuePairTraits : public ReferenceNameValuePair<T> {
 public:
  using ReferenceNameValuePair<T>::ReferenceNameValuePair;

  const ArgumentTraits& argument_traits() const { return argument_traits_; }

  ReferenceNameValuePairTraits& help(const char* text) {
    argument_traits_.help = text;
    return *this;
  }

  ReferenceNameValuePairTraits& label(const char* text) {
    argument_traits_.label = text;
    return *this;
  }

 private:
  ArgumentTraits argument_traits_;
};

template <typename T>
ReferenceNameValuePairTraits<T> MakeNameValuePairTraits(T* value, const char* name) {
  return ReferenceNameValuePairTraits<T>(value, name);
}

#define MJ_NVPT(x) mjlib::base::MakeNameValuePairTraits(&x, #x)

}
}
