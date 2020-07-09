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

#include "mjlib/telemetry/emit_json.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/test/all_types_struct.h"

#include "mjlib/telemetry/binary_write_archive.h"
#include "mjlib/telemetry/binary_schema_parser.h"

using namespace mjlib;

BOOST_AUTO_TEST_CASE(EmitJsonTest) {
  const base::test::AllTypesTest all_types;
  const std::string data =
      telemetry::BinaryWriteArchive::Write(&all_types);
  const std::string schema =
      telemetry::BinarySchemaArchive::Write<base::test::AllTypesTest>();

  telemetry::BinarySchemaParser parser(schema);
  std::ostringstream ostr;
  base::BufferReadStream read_stream(data);
  telemetry::EmitJson(ostr, parser.root(), read_stream);
  BOOST_TEST(ostr.str() == R"XX({"value_bool" : false, "value_i8" : -1, "value_i16" : -2, "value_i32" : -3, "value_i64" : -4, "value_u8" : 5, "value_u16" : 6, "value_u32" : 7, "value_u64" : 8, "value_f32" : 9, "value_f64" : 10, "value_bytes" : "CwwN", "value_str" : "de", "value_object" : {"value_u32" : 3}, "value_enum" : "kValue1", "value_array" : [{"value_u32" : 3}], "value_fixedarray" : [14, 15], "value_optional" : 21, "value_timestamp" : "1970-Jan-01 00:00:01", "value_duration" : "00:00:00.500000"})XX");
}
