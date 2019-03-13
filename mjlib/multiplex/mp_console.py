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

'''Connects to a single remote client channel using the
multiplex_protocol.'''

import argparse
import asyncio
import sys

import mjlib.micro.multiplex_protocol as mp
import mjlib.micro.aioserial as aioserial


async def make_stdin(loop, stream):
    reader = asyncio.StreamReader(loop=loop)
    reader_protocol = asyncio.StreamReaderProtocol(reader)
    await loop.connect_read_pipe(lambda: reader_protocol, stream)

    return reader


async def read(client):
    while True:
        data = await client.read(1)
        sys.stdout.write(data.decode('latin1'))
        sys.stdout.flush()


async def write(stdin, client):
    while True:
        data = await stdin.readline()
        client.write(data)
        await client.drain()


async def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d', '--device', type=str, default='/dev/ttyUSB0',
                        help='serial device')
    parser.add_argument('-b', '--baud', type=int, default=3000000,
                        help='baud rate')
    parser.add_argument('-t', '--target', type=int, default=1,
                        help='destination multiplex address')
    parser.add_argument('-c', '--channel', type=int, default=1,
                        help='destination multiples channel')
    args = parser.parse_args()

    serial = aioserial.AioSerial(port=args.device, baudrate=args.baud)
    manager = mp.MultiplexManager(serial)
    mc = mp.MultiplexClient(
        manager,
        destination_id=args.target,
        channel=args.channel)

    stdin = await make_stdin(asyncio.get_event_loop(), sys.stdin)

    await asyncio.gather(
        read(mc),
        write(stdin, mc))


if __name__ == '__main__':
    asyncio.get_event_loop().run_until_complete(main())
