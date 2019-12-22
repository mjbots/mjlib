#!/usr/bin/python3 -B

# Copyright 2019 Josh Pieper, jjp@pobox.com.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import io
import unittest


import mjlib.telemetry.reader as reader


class _TestType:
    def __init__(self, tester):
        self.tester = tester

    def __call__(self, type_class, data):
        return self.tester(type_class, data)


class ReaderTest(unittest.TestCase):
    def test_basic(self):
        _TESTS = [
            (bytes([1]), reader.NullType, bytes([]), None),
            (bytes([2]), reader.BooleanType, bytes([0]), False),
            (bytes([3, 2]), reader.FixedIntType, bytes([1, 2]), 0x201),
            (bytes([4, 4]), reader.FixedUIntType, bytes([1, 2, 3, 4]), 0x04030201),
            (bytes([6]), reader.VaruintType, bytes([3]), 3),
            (bytes([7]), reader.Float32Type, bytes([0, 0, 0, 0]), 0.0),
            (bytes([8]), reader.Float64Type, bytes([0] * 8), 0.0),
            (bytes([9]), reader.BytesType, bytes([2, 4, 5]), bytes([4, 5])),
            (bytes([10]), reader.StringType, bytes([3, 97, 98, 99]), 'abc'),
            (bytes([16, 0,
                    0,
                      2, 102, 49,
                      0,
                      2,
                      0,
                    0,
                      0,
                      0,
                      0,
                      0,
            ]), reader.ObjectType, bytes([0]),
             _TestType(lambda type_class, value :
                       value == type_class.namedtuple(False))),
        ]

        for (schema_data, expected_type, data_data, data_value) in _TESTS:
            actual_type = reader.Type.from_binary(io.BytesIO(schema_data))
            self.assertTrue(isinstance(actual_type, expected_type))

            actual_value = actual_type.read(reader.Stream(io.BytesIO(data_data)))

            if isinstance(data_value, _TestType):
                self.assertTrue(data_value(actual_type, actual_value))
            else:
                self.assertEqual(actual_value, data_value)


if __name__ == '__main__':
    unittest.main()
