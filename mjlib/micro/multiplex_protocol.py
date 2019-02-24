#!/usr/bin/python3 -B

# Copyright 2015-2019 Josh Pieper, jjp@pobox.com.
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

'''Implements the multiplex_protocol protocol for python3 asyncio.'''

import asyncio
import binascii
import inspect
import io
import struct

import mjlib.micro.stream_helpers as stream_helpers


_FRAME_HEADER_STRUCT = struct.Struct('<HBB')
_FRAME_HEADER_MAGIC = 0xab54
_FRAME_RESPONSE_REQUEST = 0x80

_STREAM_CLIENT_TO_SERVER = 0x40
_STREAM_SERVER_TO_CLIENT = 0x41


async def read_varuint(stream):
    result = 0
    shift = 0

    for i in range(5):
        data = await stream.read(1)
        this_byte, = struct.unpack('<B', data)
        result |= (this_byte & 0x7f) << shift
        shift += 7

        if (this_byte & 0x80) == 0:
            return result

    assert False


class MultiplexManager:
    def __init__(self, stream, source_id=0):
        '''One MultiplexManager must exist for the underlying stream that all
        multiplex clients rely on.

        stream - an asyncio capable file like object
        '''
        self.stream = stream
        self.source_id = source_id
        self.lock = asyncio.Lock()
        self._write_data = bytearray()

    def write(self, data):
        self._write_data += data

    async def drain(self):
        async with self.lock:
            to_write, self._write_data = self._write_data, bytearray()
            self.stream.write(to_write)
            await self.stream.drain()


class MultiplexClient:
    def __init__(self, manager, destination_id,
                 channel=1,
                 poll_rate_s=0.1,
                 timeout=0.05):
        '''destination_id - a 7 bit identifier of the remote device to
        communicate with
        '''
        self._manager = manager
        self._destination_id = destination_id
        self._channel = channel
        self._poll_rate_s = poll_rate_s
        self._timeout = timeout
        self._read_data = bytearray()

    def write(self, data, **kwargs):
        header = _FRAME_HEADER_STRUCT.pack(
            _FRAME_HEADER_MAGIC,
            self._manager.source_id,
            self._destination_id)

        assert len(data) < 100
        payload = struct.pack(
            '<BBB',
            _STREAM_CLIENT_TO_SERVER,
            self._channel,
            len(data)) + data

        frame_minus_crc = header + struct.pack('<B', len(payload)) + payload
        crc = binascii.crc_hqx(frame_minus_crc, 0xffff)
        frame = frame_minus_crc + struct.pack('<H', crc)

        self._manager.write(frame, **kwargs)

    async def drain(self):
        await self._manager.drain()

    async def read(self, size):
        # Poll repeatedly until we have enough.
        while len(self._read_data) < size:
            async with self._manager.lock:
                result = await self._try_one_poll()
                if result is not None:
                    self._read_data += result

            if len(self._read_data) >= size:
                break

            # We didn't get anything, so wait our polling period and
            # try again.
            await asyncio.sleep(self._poll_rate_s)

        to_return, self._read_data = (
            self._read_data[0:size], self._read_data[size:])
        return to_return

    def _make_stream_client_to_server(self, response, data):
        '''Returns a tuple of (target_message, remaining_data)'''
        header = _FRAME_HEADER_STRUCT.pack(
            _FRAME_HEADER_MAGIC,
            (_FRAME_RESPONSE_REQUEST if response else 0x00) |
            self._manager.source_id,
            self._destination_id)

        write_size = min(len(data), 100)
        payload = struct.pack(
            '<BBB',
            _STREAM_CLIENT_TO_SERVER,
            self._channel,
            write_size) + data[0:write_size]

        # So that we don't need a varuint.
        assert len(payload) < 127
        frame_minus_crc = header + struct.pack('<B', len(payload)) + payload
        crc = binascii.crc_hqx(frame_minus_crc, 0xffff)
        frame = frame_minus_crc + struct.pack('<H', crc)

        return frame, data[write_size:]

    async def _try_one_poll(self):
        assert self._manager.lock.locked()

        frame, _ = self._make_stream_client_to_server(True, b'')
        self._manager.stream.write(frame)
        await self._manager.stream.drain()

        recording_stream = stream_helpers.RecordingStream(self._manager.stream)

        try:
            result_frame_header = await asyncio.wait_for(
                recording_stream.read(_FRAME_HEADER_STRUCT.size),
                timeout=self._timeout)
        except asyncio.TimeoutError:
            # We treat a timeout, for now, the same as if the client
            # came back with no data at all.
            return

        header, source, dest = _FRAME_HEADER_STRUCT.unpack(result_frame_header)
        if header != _FRAME_HEADER_MAGIC:
            print('multiplex_protocol: re-synchronizing! {:04x}'.format(header),
                  flush=True)
            # We appear to be unsynchronized with one or more
            # receivers.
            _ = await recording_stream.read(8192, block=False)

            # Return nothing and hope for better next time.
            return

        async def read_payload():
            payload_len = await read_varuint(recording_stream)
            sizeof_crc = 2
            payload_and_crc = await recording_stream.read(
                payload_len + sizeof_crc)
            return payload_and_crc

        payload_and_crc = await asyncio.wait_for(
            read_payload(), timeout=self._timeout)

        if dest != self._manager.source_id:
            # Interesting.  This shouldn't really happen.
            #
            # TODO(jpieper): Log this.
            return

        if source != self._destination_id:
            return

        result_frame = recording_stream.buffer()

        # TODO(jpieper): Verify CRC.

        payload = payload_and_crc[0:-2]
        if len(payload) < 3:
            return

        payload_stream = stream_helpers.AsyncStream(io.BytesIO(payload))
        subframe_id = await read_varuint(payload_stream)
        channel = await read_varuint(payload_stream)
        server_len = await read_varuint(payload_stream)

        if subframe_id is None or channel is None or server_len is None:
            return

        if subframe_id != _STREAM_SERVER_TO_CLIENT:
            return

        return payload[payload_stream.tell():]
