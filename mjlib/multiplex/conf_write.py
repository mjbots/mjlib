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

'''Applies a series of PersistentConfig commands to an embedded
device.'''

import argparse
import asyncio


import mjlib.multiplex.multiplex_protocol as mp
import mjlib.multiplex.aioserial as aioserial


async def readline(stream):
    result = bytearray()
    while True:
        char = await stream.read(1)
        if char == b'\r' or char == b'\n':
            if len(result):
                return result
        else:
            result += char


async def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=str,
                        help='file containing configuration commands')
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
        timeout=0.3,  # "conf write" can take a long time
        destination_id=args.target,
        channel=args.channel)

    with open(args.data, 'rb') as data:
        for line in data.readlines():
            if line.strip() == '':
                continue

            print(line.strip().decode('latin1'))
            mc.write(line)
            await mc.drain()

            result = (await readline(mc)).strip()
            print('>', result.decode('latin1'))
            if result != b'OK':
                raise RuntimeError('Unknown response: ' + result)


if __name__ == '__main__':
    asyncio.get_event_loop().run_until_complete(main())
