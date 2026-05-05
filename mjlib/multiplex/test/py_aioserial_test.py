#!/usr/bin/python3 -B

# Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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


import unittest

from mjlib.multiplex import aioserial


class AioSerialLoopSetterTest(unittest.TestCase):
    def test_loop_setter_writes_backing_attribute(self):
        # AioSerial.__init__ touches a real serial port, so build the
        # instance without running it and exercise the setter directly.
        # Pre-fix this recurses to RecursionError.
        instance = aioserial.AioSerial.__new__(aioserial.AioSerial)
        instance._loop = None
        sentinel = object()

        aioserial.AioSerial.loop.fset(instance, sentinel)
        self.assertIs(instance._loop, sentinel)


if __name__ == '__main__':
    unittest.main()
