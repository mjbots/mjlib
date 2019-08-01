// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

namespace mjlib {
namespace base {

/// This class template can be used to rank multiple overloads.
///
/// The functions to be called are declared like:
///
///   void MyFunction(int regular, int args, PriorityTag<0>);  // fallback
///   void MyFunction(int regular, int args, PriorityTag<1>);  // preferred
///
/// Then it can be called like:
///
///   MyFunction(0, 1, PriorityTag<1>());
template <size_t I> struct PriorityTag : PriorityTag<I-1> {};
template <> struct PriorityTag<0> {};

}
}
