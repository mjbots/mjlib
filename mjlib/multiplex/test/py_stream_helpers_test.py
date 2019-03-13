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


import asyncio
import io
import unittest


import mjlib.multiplex.stream_helpers as sh

def _run(coro):
    return asyncio.get_event_loop().run_until_complete(coro)


class RecordingStreamTest(unittest.TestCase):
    def test_basic(self):
        stream = sh.AsyncStream(io.BytesIO(bytes([0x05, 0x06, 0x07, 0x08])))
        dut = sh.RecordingStream(stream)

        async def run():
            return await dut.read(1)

        result = _run(run())
        self.assertEqual(result, bytes([0x05]))
        self.assertEqual(dut.buffer(), bytes([0x05]))

        result2 = _run(run())
        self.assertEqual(result2, bytes([0x06]))
        self.assertEqual(dut.buffer(), bytes([0x05, 0x06]))

        result3 = _run(run())
        self.assertEqual(result3, bytes([0x07]))
        self.assertEqual(dut.buffer(), bytes([0x05, 0x06, 0x07]))


class PipeStreamTest(unittest.TestCase):
    async def async_test_basic(self):
        dut = sh.PipeStream()

        async def write(pipe):
            pipe.write(bytes([4, 5, 6]))
            await pipe.drain()

        async def read(pipe):
            return await pipe.read(3)

        results = await asyncio.gather(write(dut.side_a), read(dut.side_b))

        self.assertEqual(results[1], bytes([4, 5, 6]))

    def test_basic(self):
        # import pdb; pdb.set_trace()
        _run(asyncio.Task(self.async_test_basic()))


if __name__ == '__main__':
    unittest.main()
