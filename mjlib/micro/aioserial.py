# Copyright 2018 Henry Chang
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import array
import asyncio
import concurrent.futures
import sys
from typing import List, Optional, Union

import serial


class AioSerial:

    def __init__(
            self,
            port: Optional[str] = None,
            baudrate: int = 9600,
            bytesize: int = serial.EIGHTBITS,
            parity: str = serial.PARITY_NONE,
            stopbits: Union[float, int] = serial.STOPBITS_ONE,
            timeout: Optional[float] = None,
            xonxoff: bool = False,
            rtscts: bool = False,
            write_timeout: Optional[float] = None,
            dsrdtr: bool = False,
            inter_byte_timeout: Optional[float] = None,
            exclusive: Optional[bool] = None,
            loop: Optional[asyncio.AbstractEventLoop] = None,
            **kwargs):
        self.serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=bytesize,
            parity=parity,
            stopbits=stopbits,
            timeout=timeout,
            xonxoff=xonxoff,
            rtscts=rtscts,
            write_timeout=write_timeout,
            dsrdtr=dsrdtr,
            inter_byte_timeout=inter_byte_timeout,
            exclusive=exclusive,
            **kwargs)
        self._loop: Optional[asyncio.AbstractEventLoop] = loop
        self._read_executor = \
            concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self._write_executor = \
            concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self._write_data = bytearray()

    @property
    def loop(self) -> Optional[asyncio.AbstractEventLoop]:
        return self._loop if self._loop else asyncio.get_running_loop() \
                if sys.version_info >= (3, 7) else asyncio.get_event_loop()

    @loop.setter
    def loop(self, value: Optional[asyncio.AbstractEventLoop]):
        self.loop = value

    async def read(self, size: int = 1) -> bytes:
        return await self.loop.run_in_executor(
            self._read_executor, self.serial.read, size)

    def write(self, data: Union[bytearray, bytes, memoryview]) -> int:
        self._write_data += data

    async def drain(self, ) -> int:
        to_write, self._write_data = self._write_data, bytearray()
        return await self.loop.run_in_executor(
            self._write_executor, self.serial.write, to_write)
