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
import inspect
import io

class _Buffer:
    def __init__(self):
        self.data = bytes()
        self.cond = asyncio.Condition()


class _PipeSide:
    def __init__(self, write, read):
        self._write = write
        self._read = read

    async def read(self, size):
        async with self._read.cond:
            def predicate():
                result = len(self._read.data) >= size
                return result
            await self._read.cond.wait_for(predicate)
            to_return, self._read.data = (
                self._read.data[0:size], self._read.data[size:])
            self._read.cond.notify_all()

        return to_return

    def write(self, data):
        self._write.data += data

    async def drain(self):
        async with self._write.cond:
            self._write.cond.notify_all()
            await self._write.cond.wait_for(lambda: len(self._write.data) == 0)


class PipeStream:
    '''An asynchronous pipe.  The two sides of the pipe can be accessed
       with:

       instance.side_a
         and
       instance.side_b
    '''
    def __init__(self):
        self._side_a_buffer = _Buffer()
        self._side_b_buffer = _Buffer()

        self.side_a = _PipeSide(self._side_a_buffer, self._side_b_buffer)
        self.side_b = _PipeSide(self._side_b_buffer, self._side_a_buffer)


class AsyncStream:
    '''Wraps a synchronous stream so that it can be used in await
    contexts.'''
    def __init__(self, base):
        self._base = base

    async def read(self, size):
        return self._base.read(size)

    def write(self, data):
        return self._base.write(size)

    async def drain(self):
        return

    def tell(self):
        return self._base.tell()


class RecordingStream:
    '''Models an async io.BytesIO backed by a different stream.  All reads
    are recorded into an internal buffer.

    It transparently works with base objects that are async enabled
    and those that aren't, presenting an async interface to both.
    '''

    def __init__(self, base):
        self._base = base
        self._buffer = io.BytesIO()

    async def read(self, size, **kwargs):
        result = await self._base.read(size, **kwargs)
        self._buffer.write(result)
        return result

    def buffer(self):
        return self._buffer.getvalue()

    def tell(self):
        return self._base.tell()
