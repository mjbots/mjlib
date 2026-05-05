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


import asyncio
import unittest

from mjlib.multiplex import aioserial


def _make_bare_aioserial():
    # AioSerial.__init__ touches a real serial port; build the
    # instance without running it so the tests can poke at the
    # plumbing in isolation.
    instance = aioserial.AioSerial.__new__(aioserial.AioSerial)
    instance._loop = None
    instance._read_data = bytearray()
    instance._write_data = bytearray()
    instance._read_event = asyncio.Event()
    return instance


# Python 3.14 no longer auto-creates an event loop in the main thread on
# get_event_loop(). Create one explicitly so this also works on Python 3.10
# (Ubuntu 22.04), where get_event_loop() did create one implicitly.
_LOOP = asyncio.new_event_loop()
asyncio.set_event_loop(_LOOP)

def _run(coro):
    return _LOOP.run_until_complete(coro)


class AioSerialLoopSetterTest(unittest.TestCase):
    def test_loop_setter_writes_backing_attribute(self):
        # Pre-fix this recurses to RecursionError.
        instance = _make_bare_aioserial()
        sentinel = object()

        aioserial.AioSerial.loop.fset(instance, sentinel)
        self.assertIs(instance._loop, sentinel)


class AioSerialReadSizeZeroTest(unittest.TestCase):
    def test_read_zero_with_buffered_data_returns_empty(self):
        # Pre-fix the assertion `read_size > 0` fired when size==0
        # and any data was buffered.
        instance = _make_bare_aioserial()
        instance._read_data = bytearray(b'hello')

        result = _run(instance.read(0))
        self.assertEqual(result, bytearray())
        # Buffered bytes must not be consumed.
        self.assertEqual(instance._read_data, bytearray(b'hello'))

    def test_read_zero_with_empty_buffer_returns_empty(self):
        # Pre-fix this blocked forever on the read_event because the
        # zero-size case was not short-circuited.
        instance = _make_bare_aioserial()

        result = _run(asyncio.wait_for(instance.read(0), timeout=0.5))
        self.assertEqual(result, bytearray())


if __name__ == '__main__':
    unittest.main()
