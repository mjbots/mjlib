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
import mjlib.multiplex.multiplex_protocol as mp


def _run(coro):
    return asyncio.get_event_loop().run_until_complete(coro)


class MultiplexProtocolTest(unittest.TestCase):
    def test_read_varuint(self):
        test_cases = [
            ([0x05], 0x05),
            ([0x05, 0x06], 0x05),
            ([0x85, 0x01], 0x85),
            ([0x85, 0x81, 0x01], 0x4085),
        ]

        for data, expected in test_cases:
            async def run():
                stream = sh.AsyncStream(io.BytesIO(bytes(data)))
                return await mp.read_varuint(stream)

            result = _run(run())
            self.assertEqual(result, expected)

    async def async_test_multiplex_client_read(self):
        pipe = sh.PipeStream()
        mm = mp.MultiplexManager(pipe.side_a)
        mc1 = mp.MultiplexClient(mm, 5)

        async def server():
            # First read one polling packet.
            poll = await pipe.side_b.read(
                2 +  # magic
                1 +  # source id
                1 +  # dest id
                1 +  # payload size
                1 +  # subframe type
                1 +  # channel
                1 +  # number of bytes (0)
                2)  # crc

            # Then write a response.
            pipe.side_b.write(bytes([
                0x54, 0xab,
                0x05,
                0x00,
                0x07,
                0x41,
                0x01,
                0x04,
                ord('t'), ord('e'), ord('s'), ord('t'),
                0x00, 0x00,
            ]))

            await pipe.side_b.drain()

        async def client():
            return await mc1.read(4)

        results = await asyncio.gather(server(), client())
        self.assertEqual(results[1], b'test')

    def test_multiplex_client_read(self):
        _run(self.async_test_multiplex_client_read())

    async def async_test_multiplex_client_write(self):
        pipe = sh.PipeStream()
        mm = mp.MultiplexManager(pipe.side_a)
        mc1 = mp.MultiplexClient(mm, 5)

        async def server():
            data = await pipe.side_b.read(
                2 +  # magic
                1 +  # source id
                1 +  # dest id
                1 +  # payload size
                1 +  # subframe type
                1 +  # channel
                1 +  # number of bytes
                4 +  # data
                2)  # crc
            return data

        async def client():
            mc1.write(b'test')
            await mc1.drain()

        results = await asyncio.gather(server(), client())
        self.assertEqual(results[0], bytes([
            0x54, 0xab,
            0x00,
            0x05,
            0x07,
            0x40,
            0x01,
            0x04,
            ord('t'), ord('e'), ord('s'), ord('t'),
            0x01, 0xa0]))

    def test_multiplex_client_write(self):
        _run(self.async_test_multiplex_client_write())


class RegisterTest(unittest.TestCase):
    async def async_test_simple_parse_register(self):
        result = await mp.ParseRegisterReply(bytes([0x20, 0x10, 0x08]))
        self.assertTrue(0x10 in result)
        self.assertEqual(result[0x10], mp.RegisterValue(0x08, 0))
        self.assertEqual(len(result), 1)

        result = await mp.ParseRegisterReply(
            bytes([0x21, 0x10, 0x02, 0x03]))
        self.assertTrue(0x10 in result)
        self.assertEqual(result[0x10], mp.RegisterValue(0x0302, 1))

        result = await mp.ParseRegisterReply(
            bytes([0x22, 0x10, 0x03, 0x04, 0x05, 0x06]))
        self.assertTrue(0x10 in result)
        self.assertEqual(result[0x10], mp.RegisterValue(0x06050403, 2))

        result = await mp.ParseRegisterReply(
            bytes([0x23, 0x10, 0x00, 0x00, 0x00, 0x00]))
        self.assertTrue(0x10 in result)
        self.assertEqual(result[0x10], mp.RegisterValue(0.0, 3))

        result = await mp.ParseRegisterReply(
            bytes([0x24, 0x10, 0x03, 0x04, 0x05, 0x06]))
        self.assertEqual(result,
                         { 0x10 : mp.RegisterValue(0x04, 0),
                           0x11 : mp.RegisterValue(0x05, 0),
                           0x12 : mp.RegisterValue(0x06, 0) })

    def test_simple_parse_register(self):
        _run(self.async_test_simple_parse_register())


if __name__ == '__main__':
    unittest.main()
