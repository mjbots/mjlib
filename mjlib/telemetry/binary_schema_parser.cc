// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/telemetry/binary_schema_parser.h"

#include <deque>

namespace mjlib {
namespace telemetry {

class BinarySchemaParser::Impl {
 public:
  Impl(std::string_view, std::string_view) {
  }

  const Element* root() const {
    return root_;
  }

  void SkipTo(const Element*, base::ReadStream&) {
  }

  const Element* root_;

  // We use a deque, so push_back doesn't invalidate existing
  // iterators.
  std::deque<Element> elements_;
};

BinarySchemaParser::BinarySchemaParser(std::string_view schema,
                                       std::string_view record_name)
    : impl_(std::make_unique<Impl>(schema, record_name)) {}

BinarySchemaParser::~BinarySchemaParser() {}

const BinarySchemaParser::Element* BinarySchemaParser::root() const {
  return impl_->root();
}

void BinarySchemaParser::SkipTo(const Element* element,
                                base::ReadStream& stream) const {
  impl_->SkipTo(element, stream);
}

}
}
