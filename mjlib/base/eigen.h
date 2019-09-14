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

#include <array>

#include <Eigen/Dense>

#include "mjlib/base/visitor.h"

/// @file
///
/// This provides serialization support for Eigen matrix types.
///
/// Row or column vectors are serialized as a flat array.  Everything
/// else gets a 2-d array in column major order.

namespace mjlib {
namespace base {

template <typename Scalar, int RowsAtCompileTime, int ColsAtCompileTime,
          int Options>
struct ExternalSerializer<
  Eigen::Matrix<Scalar, RowsAtCompileTime, ColsAtCompileTime, Options>> {
  using Matrix =
      Eigen::Matrix<Scalar, RowsAtCompileTime, ColsAtCompileTime, Options>;

  template <typename PairReceiver>
  void Serialize(Matrix* matrix, PairReceiver receiver) {
    // We don't support row-major yet.
    static_assert(Options == 0);

    if (RowsAtCompileTime == 1 || ColsAtCompileTime == 1) {
      using EquivalentArray =
          std::array<Scalar, std::max(RowsAtCompileTime, ColsAtCompileTime)>;
      EquivalentArray& data =
          *reinterpret_cast<EquivalentArray*>(matrix->data());

      receiver(MJ_NVP(data));
    } else {
      using EquivalentArray =
          std::array<std::array<Scalar, RowsAtCompileTime>, ColsAtCompileTime>;
      EquivalentArray& data =
          *reinterpret_cast<EquivalentArray*>(matrix->data());

      receiver(MJ_NVP(data));
    }
  }
};

}
}
