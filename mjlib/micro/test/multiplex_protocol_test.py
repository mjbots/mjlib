#!/usr/bin/python3 -B

# Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

import mjlib.micro.multiplex_protocol as mp


class MultiplexProtocolTest(unittest.TestCase):
    def test_read_varuint(self):
        stream = io.BytesIO([0x00])
        result = mp.read_varuint(stream)
        self.assertEqual(result, 0)


if __name__ == '__main__':
    unittest.main()
