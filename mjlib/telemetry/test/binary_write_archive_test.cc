// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/telemetry/binary_write_archive.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/fast_stream.h"

#include "mjlib/base/test/all_types_struct.h"

#include "mjlib/telemetry/test/test_util.h"

using namespace mjlib;

BOOST_AUTO_TEST_CASE(BinaryWriteArchive) {
  base::FastOStringStream ostr;
  telemetry::BinaryWriteArchive dut(ostr);
  const base::test::AllTypesTest all_types;
  dut.Accept(&all_types);

  const std::vector<uint8_t> expected = {
    0x00,  // value_bool : false
    0xff,  // value_i8 : -1
    0xfe, 0xff,  // value_i16 : -2
    0xfd, 0xff, 0xff, 0xff,  // value_i32 : -3
    0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // value_i64 : -4
    0x05,  // value_u8 : 5
    0x06, 0x00,  // value_u16 : 6
    0x07, 0x00, 0x00, 0x00,  // value_u32 : 7
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_u64 : 8
    0x00, 0x00, 0x10, 0x41,  // value_f32 : 9.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40,  // value_f64 : 10.0
    0x03, 0x0b, 0x0c, 0x0d,  // bytes = [0x0b, 0x0c]
    0x02, 'd', 'e',  // value_str : "de"
    0x03, 0x00, 0x00, 0x00,  // SubTest1::value_u32 : 3
    0x00,  // value_enum : 0
    0x01,  0x03, 0x00, 0x00, 0x00,  // value_array : [ SubTest1() ]
    0x0e, 0x0f,  // value_fixedarray : [ 14, 15 ]
    0x01, 0x15, 0x00, 0x00, 0x00,  // value_optional : 21
    0x40, 0x42, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_timestamp
    0x20, 0xa1, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,  // value_duration
  };

  telemetry::test::Compare(expected, ostr.str());
  telemetry::test::Compare(
      expected, telemetry::BinaryWriteArchive::Write(all_types));
}

BOOST_AUTO_TEST_CASE(BinarySchemaArchive) {
  base::FastOStringStream ostr;
  telemetry::BinarySchemaArchive dut(ostr);
  base::test::AllTypesTest all_types;
  dut.Accept(&all_types);

  const std::vector<uint8_t> expected = {
    0x10,  // kObject
     0x00,  // ObjectFlags
      0x00,  // FieldFlags
       0x0a, 'v', 'a', 'l', 'u', 'e', '_', 'b', 'o', 'o', 'l',
       0x00,  // naliases
       0x02,  // kBool
       0x01, 0x00,  // default : false

      0x00,  // FieldFlags
       0x08, 'v', 'a', 'l', 'u', 'e', '_', 'i', '8',
       0x00,  // naliases
       0x03, 0x01,  // fixedint_8
       0x01, 0xff,  // default: -1

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '1', '6',
       0x00,  // naliases
       0x03, 0x02,  // fixedint_16
       0x01, 0xfe, 0xff,  // default: -2

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '3', '2',
       0x00,  // naliases
       0x03, 0x04,  // fixedint_32
       0x01, 0xfd, 0xff, 0xff, 0xff,  // default: -3

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '6', '4',
       0x00,  // naliases
       0x03, 0x08,  // fixedint_32
       0x01, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // default: -4

      0x00,  // FieldFlags
       0x08, 'v', 'a', 'l', 'u', 'e', '_', 'u', '8',
       0x00,  // naliases
       0x04, 0x01,  // fixeduint_8
       0x01, 0x05,  // default: 5

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '1', '6',
       0x00,  // naliases
       0x04, 0x02,  // fixeduint_16
       0x01, 0x06, 0x00,  // default: 6

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',
       0x00,  // naliases
       0x04, 0x04,  // fixeduint_32
       0x01, 0x07, 0x00, 0x00, 0x00,  // default: 7

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '6', '4',
       0x00,  // naliases
       0x04, 0x08,  // fixeduint_64
       0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // default: 8

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'f', '3', '2',
       0x00,  // naliases
       0x07,  // float32
       0x01, 0x00, 0x00, 0x10, 0x41,  // default : 9.0

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'f', '6', '4',
       0x00,  // naliases
       0x08,  // float64
       0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40,  // default: 10.0

      0x00,  // FieldFlags
       0x0b, 'v', 'a', 'l', 'u', 'e', '_', 'b', 'y', 't', 'e', 's',
       0x00,  // naliases
       0x09,  // bytes
       0x01, 0x03, 0x0b, 0x0c, 0x0d,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 's', 't', 'r',
       0x00,  // naliases
       0x0a,  // str
       0x01, 0x02, 'd', 'e',

      0x00,  // FieldFlags
       0x0c, 'v', 'a', 'l', 'u', 'e', '_', 'o', 'b', 'j', 'e', 'c', 't',
       0x00,  // naliases
       0x10,  // object

        0x00,  // ObjectFlags

         0x00,  // FieldFlags
          0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',  // name
          0x00,  // aliases
          0x04, 0x04,  // fixeduint32
          0x01, 0x03, 0x00, 0x00, 0x00,  // default : 3

         0x00,  // FieldFlags
          0x00,  // name
          0x00,  // aliases
          0x00,  // kFinal
          0x00,  // no default

       0x01, 0x03, 0x00, 0x00, 0x00,  // default : SubTest1{3}

      0x00,  // FieldFlags
       0x0a, 'v', 'a', 'l', 'u', 'e', '_', 'e', 'n', 'u', 'm',
       0x00,  // naliases
       0x11,  // enum
       0x06,  // kVaruint
       0x03,  // nvalues
        0x00,  0x07, 'k', 'V', 'a', 'l', 'u', 'e', '1',
        0x05,  0x0a, 'k', 'N', 'e', 'x', 't', 'V', 'a', 'l', 'u', 'e',
        0x14,  0x0d, 'k', 'A', 'n', 'o', 't', 'h', 'e', 'r',
                     'V', 'a', 'l', 'u', 'e',
       0x01, 0x00,  // default : 0

      0x00,  // FieldFlags
       0x0b, 'v', 'a', 'l', 'u', 'e', '_', 'a', 'r', 'r', 'a', 'y',
       0x00,  // naliases
       0x12,  // array

        0x10,  // object

         0x00,  // ObjectFlags

          0x00,  // FieldFlags
           0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',  // name
           0x00,  // aliases
           0x04, 0x04,  // fixeduint32
           0x01, 0x03, 0x00, 0x00, 0x00,  // default : 3

          0x00,  // FieldFlags
           0x00,  // name
           0x00,  // aliases
           0x00,  // kFinal
           0x00,  // no default

        0x01, 0x01, 0x03, 0x00, 0x00, 0x00, // default : [ SubTest1{3} ]

      0x00,  // FieldFlags
      0x10, 'v', 'a', 'l', 'u', 'e', '_', 'f', 'i', 'x', 'e', 'd',
            'a', 'r', 'r', 'a', 'y',
       0x00,  // naliases
       0x13,  // fixedarray
         0x02,  // size=2
         0x04, 0x01,  // uint8
       0x01, 0x0e, 0x0f,  // default: [ 14, 15 ]


      0x00,  // FieldFlags
       0x0e, 'v', 'a', 'l', 'u', 'e', '_', 'o', 'p', 't', 'i', 'o', 'n', 'a', 'l',
       0x00,  // naliases
       0x15,  // union

        0x01,  // null
        0x03, 0x04,  // fixedint32
        0x00,  // final

        0x01, 0x01, 0x15, 0x00, 0x00, 0x00,  // default: 21


      0x00,  // FieldFlags
       0x0f, 'v', 'a', 'l', 'u', 'e', '_',
             't', 'i', 'm', 'e', 's', 't', 'a', 'm', 'p',
       0x00,  // naliases
       0x16,  // timestamp
       0x01, 0x40, 0x42, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,

      0x00,  // FieldFlags
       0x0e, 'v', 'a', 'l', 'u', 'e', '_', 'd', 'u', 'r', 'a', 't', 'i', 'o', 'n',
       0x00,  // naliases
       0x17,  // timestamp
       0x01, 0x20, 0xa1, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,

      0x00,  // FieldFlags
       0x00,  // name
       0x00,  // aliases
       0x00,  // kFinal
       0x00,  // no default
  };

  telemetry::test::Compare(expected, ostr.str());
  telemetry::test::Compare(
      expected, telemetry::BinarySchemaArchive::schema<
      base::test::AllTypesTest>());

  telemetry::test::Compare(
      expected,
      telemetry::BinarySchemaArchive::Write<base::test::AllTypesTest>());
}

BOOST_AUTO_TEST_CASE(BinarySchemaArchiveNoDefault) {
  base::FastOStringStream ostr;
  telemetry::BinarySchemaArchive::Options options;
  options.emit_default = false;

  telemetry::BinarySchemaArchive dut(ostr, options);
  base::test::AllTypesTest all_types;
  dut.Accept(&all_types);

  const std::vector<uint8_t> expected = {
    0x10,  // kObject
     0x00,  // ObjectFlags
      0x00,  // FieldFlags
       0x0a, 'v', 'a', 'l', 'u', 'e', '_', 'b', 'o', 'o', 'l',
       0x00,  // naliases
       0x02,  // kBool
       0x00,

      0x00,  // FieldFlags
       0x08, 'v', 'a', 'l', 'u', 'e', '_', 'i', '8',
       0x00,  // naliases
       0x03, 0x01,  // fixedint_8
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '1', '6',
       0x00,  // naliases
       0x03, 0x02,  // fixedint_16
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '3', '2',
       0x00,  // naliases
       0x03, 0x04,  // fixedint_32
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'i', '6', '4',
       0x00,  // naliases
       0x03, 0x08,  // fixedint_32
       0x00,

      0x00,  // FieldFlags
       0x08, 'v', 'a', 'l', 'u', 'e', '_', 'u', '8',
       0x00,  // naliases
       0x04, 0x01,  // fixeduint_8
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '1', '6',
       0x00,  // naliases
       0x04, 0x02,  // fixeduint_16
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',
       0x00,  // naliases
       0x04, 0x04,  // fixeduint_32
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '6', '4',
       0x00,  // naliases
       0x04, 0x08,  // fixeduint_64
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'f', '3', '2',
       0x00,  // naliases
       0x07,  // float32
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 'f', '6', '4',
       0x00,  // naliases
       0x08,  // float64
       0x00,

      0x00,  // FieldFlags
       0x0b, 'v', 'a', 'l', 'u', 'e', '_', 'b', 'y', 't', 'e', 's',
       0x00,  // naliases
       0x09,  // bytes
       0x00,

      0x00,  // FieldFlags
       0x09, 'v', 'a', 'l', 'u', 'e', '_', 's', 't', 'r',
       0x00,  // naliases
       0x0a,  // str
       0x00,

      0x00,  // FieldFlags
       0x0c, 'v', 'a', 'l', 'u', 'e', '_', 'o', 'b', 'j', 'e', 'c', 't',
       0x00,  // naliases
       0x10,  // object

        0x00,  // ObjectFlags

         0x00,  // FieldFlags
          0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',  // name
          0x00,  // aliases
          0x04, 0x04,  // fixeduint32
          0x00,

         0x00,  // FieldFlags
          0x00,  // name
          0x00,  // aliases
          0x00,  // kFinal
          0x00,  // no default

       0x00,

      0x00,  // FieldFlags
       0x0a, 'v', 'a', 'l', 'u', 'e', '_', 'e', 'n', 'u', 'm',
       0x00,  // naliases
       0x11,  // enum
       0x06,  // kVaruint
       0x03,  // nvalues
        0x00,  0x07, 'k', 'V', 'a', 'l', 'u', 'e', '1',
        0x05,  0x0a, 'k', 'N', 'e', 'x', 't', 'V', 'a', 'l', 'u', 'e',
        0x14,  0x0d, 'k', 'A', 'n', 'o', 't', 'h', 'e', 'r',
                     'V', 'a', 'l', 'u', 'e',
       0x00,

      0x00,  // FieldFlags
       0x0b, 'v', 'a', 'l', 'u', 'e', '_', 'a', 'r', 'r', 'a', 'y',
       0x00,  // naliases
       0x12,  // array

        0x10,  // object

         0x00,  // ObjectFlags

          0x00,  // FieldFlags
           0x09, 'v', 'a', 'l', 'u', 'e', '_', 'u', '3', '2',  // name
           0x00,  // aliases
           0x04, 0x04,  // fixeduint32
           0x00,

          0x00,  // FieldFlags
           0x00,  // name
           0x00,  // aliases
           0x00,  // kFinal
           0x00,  // no default

        0x00,

      0x00,  // FieldFlags
      0x10, 'v', 'a', 'l', 'u', 'e', '_', 'f', 'i', 'x', 'e', 'd',
            'a', 'r', 'r', 'a', 'y',
       0x00,  // naliases
       0x13,  // fixedarray
         0x02,  // size=2
         0x04, 0x01,  // uint8
       0x00,


      0x00,  // FieldFlags
       0x0e, 'v', 'a', 'l', 'u', 'e', '_', 'o', 'p', 't', 'i', 'o', 'n', 'a', 'l',
       0x00,  // naliases
       0x15,  // union

        0x01,  // null
        0x03, 0x04,  // fixedint32
        0x00,  // final

        0x00,


      0x00,  // FieldFlags
       0x0f, 'v', 'a', 'l', 'u', 'e', '_',
             't', 'i', 'm', 'e', 's', 't', 'a', 'm', 'p',
       0x00,  // naliases
       0x16,  // timestamp
       0x00,

      0x00,  // FieldFlags
       0x0e, 'v', 'a', 'l', 'u', 'e', '_', 'd', 'u', 'r', 'a', 't', 'i', 'o', 'n',
       0x00,  // naliases
       0x17,  // timestamp
       0x00,

      0x00,  // FieldFlags
       0x00,  // name
       0x00,  // aliases
       0x00,  // kFinal
       0x00,  // no default
  };

  telemetry::test::Compare(expected, ostr.str());
  telemetry::test::Compare(
      expected, telemetry::BinarySchemaArchive::schema<
      base::test::AllTypesTest>(options));

  telemetry::test::Compare(
      expected,
      telemetry::BinarySchemaArchive::Write<base::test::AllTypesTest>(options));
}
