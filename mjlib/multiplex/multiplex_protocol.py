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
import enum
import inspect
import io
import struct

import mjlib.multiplex.stream_helpers as stream_helpers


_FRAME_HEADER_STRUCT = struct.Struct('<HBB')
_FRAME_HEADER_MAGIC = 0xab54
_FRAME_RESPONSE_REQUEST = 0x80

_STREAM_CLIENT_TO_SERVER = 0x40
_STREAM_SERVER_TO_CLIENT = 0x41


_REGISTER_READ_SINGLE_I8 = 0x18
_REGISTER_READ_SINGLE_I16 = 0x19
_REGISTER_READ_SINGLE_I32 = 0x1a
_REGISTER_READ_SINGLE_FLOAT = 0x1b

_REGISTER_READ_MULTIPLE_I8 = 0x1c
_REGISTER_READ_MULTIPLE_I16 = 0x1d
_REGISTER_READ_MULTIPLE_I32 = 0x1e
_REGISTER_READ_MULTIPLE_FLOAT = 0x1f

_REGISTER_REPLY_SINGLE_I8 = 0x20
_REGISTER_REPLY_SINGLE_I16 = 0x21
_REGISTER_REPLY_SINGLE_I32 = 0x22
_REGISTER_REPLY_SINGLE_FLOAT = 0x23

_REGISTER_REPLY_MULTIPLE_I8 = 0x24
_REGISTER_REPLY_MULTIPLE_I16 = 0x25
_REGISTER_REPLY_MULTIPLE_I32 = 0x26
_REGISTER_REPLY_MULTIPLE_FLOAT = 0x27

_REGISTER_WRITE_SINGLE_I8 = 0x10
_REGISTER_WRITE_SINGLE_I16 = 0x11
_REGISTER_WRITE_SINGLE_I32 = 0x12
_REGISTER_WRITE_SINGLE_FLOAT = 0x13

_REGISTER_WRITE_MULTIPLE_I8 = 0x14
_REGISTER_WRITE_MULTIPLE_I16 = 0x15
_REGISTER_WRITE_MULTIPLE_I32 = 0x16
_REGISTER_WRITE_MULTIPLE_FLOAT = 0x17

_REGISTER_WRITE_ERROR = 0x28
_REGISTER_READ_ERROR = 0x29

async def read_varuint(stream):
    result = 0
    shift = 0

    for i in range(5):
        data = await stream.read(1)
        if len(data) < 1:
            return None
        this_byte, = struct.unpack('<B', data)
        result |= (this_byte & 0x7f) << shift
        shift += 7

        if (this_byte & 0x80) == 0:
            return result

    assert False


def write_varuint(stream, value):
    assert value >= 0 and value < 2 ** 32
    while True:
        this_byte = value & 0x7f
        value = value >> 7
        if value > 0:
            this_byte |= 0x80
        stream.write(bytes([this_byte]))
        if value == 0:
            break


def _pack_frame(source, dest, payload):
        header = _FRAME_HEADER_STRUCT.pack(
            _FRAME_HEADER_MAGIC, source, dest)

        frame_minus_crc = header + struct.pack('<B', len(payload)) + payload
        crc = binascii.crc_hqx(frame_minus_crc, 0xffff)
        frame = frame_minus_crc + struct.pack('<H', crc)

        return frame


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
        assert self.lock.locked()

        to_write, self._write_data = self._write_data, bytearray()
        self.stream.write(to_write)
        await self.stream.drain()

    async def read_frame(self, only_from=None):
        assert self.lock.locked()

        recording_stream = stream_helpers.RecordingStream(self.stream)

        result_frame_header = await recording_stream.read(_FRAME_HEADER_STRUCT.size)

        header, source, dest = _FRAME_HEADER_STRUCT.unpack(result_frame_header)
        if header != _FRAME_HEADER_MAGIC:
            print('multiplex_protocol: re-synchronizing! {:04x}'.format(header),
                  flush=True)
            # We appear to be unsynchronized with one or more
            # receivers.
            _ = await recording_stream.read(8192, block=False)

            # Report a timeout error.
            raise asyncio.TimeoutError()

        async def read_payload():
            payload_len = await read_varuint(recording_stream)
            sizeof_crc = 2
            payload_and_crc = await recording_stream.read(
                payload_len + sizeof_crc)
            return payload_and_crc

        payload_and_crc = await read_payload()

        if only_from and source != only_from:
            return None

        result_frame = recording_stream.buffer()

        # TODO(jpieper): Verify CRC.

        payload = payload_and_crc[0:-2]
        if len(payload) < 3:
            return

        return payload


class MultiplexClient:
    def __init__(self, manager, destination_id,
                 channel=1,
                 poll_rate_s=0.1,
                 timeout=0.05):
        '''A client for the stream protocol.

        destination_id - a 7 bit identifier of the remote device to
        communicate with
        '''
        self._manager = manager
        self._destination_id = destination_id
        self._channel = channel
        self._poll_rate_s = poll_rate_s
        self._timeout = timeout
        self._read_data = bytearray()

    def write(self, data, **kwargs):
        payload = struct.pack(
            '<BBB',
            _STREAM_CLIENT_TO_SERVER,
            self._channel,
            len(data)) + data

        frame = _pack_frame(self._manager.source_id,
                            self._destination_id,
                            payload)

        self._manager.write(frame, **kwargs)

    async def drain(self):
        async with self._manager.lock:
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

        result = b''
        timeout = self._timeout

        # We loop multiple times in case a previous poll timed out,
        # and replies are in-flight.
        while True:
            try:
                payload = await asyncio.wait_for(
                    self._manager.read_frame(only_from=self._destination_id),
                    timeout=timeout)
            except asyncio.TimeoutError:
                # We treat a timeout, for now, the same as if the client
                # came back with no data at all.
                break

            payload_stream = stream_helpers.AsyncStream(io.BytesIO(payload))
            subframe_id = await read_varuint(payload_stream)
            channel = await read_varuint(payload_stream)
            server_len = await read_varuint(payload_stream)

            if subframe_id is None or channel is None or server_len is None:
                break

            if subframe_id != _STREAM_SERVER_TO_CLIENT:
                break

            result += payload[payload_stream.tell():]

            # On subsequent tries through this, we barely want to wait
            # at all, we're just looking to see if something is
            # already there.
            timeout = 0.0001

        return result

    async def register_query(self, request):
        '''request should be a RegisterRequest object
        returns a dict from ParseRegisterReply
        '''
        async with self._manager.lock:
            self._manager.write(_pack_frame(
                self._manager.source_id | 0x80,
                self._destination_id,
                request.data.getbuffer()))
            await self._manager.drain()

            return await ParseRegisterReply(await self._manager.read_frame())

    async def register_write(self, request):
        async with self._manager.lock:
            self._manager.write(_pack_frame(
                self._manager.source_id,
                self._destination_id,
                request.data.getbuffer()))
            await self._manager.drain()


class RegisterType(enum.Enum):
    INT8 = 0
    INT16 = 1
    INT32 = 2
    FLOAT = 3


_TYPE_FORMAT = [
    struct.Struct('<b'),
    struct.Struct('<h'),
    struct.Struct('<i'),
    struct.Struct('<f'),
]


class RegisterRequest:
    '''Constructs subframes necessary to read or write individual
    registers.'''
    def __init__(self):
        self.data = io.BytesIO()

    def read_single(self, register, reg_type):
        write_varuint(self.data, _REGISTER_READ_SINGLE_I8 + int(reg_type))
        write_varuint(self.data, register)

    def read_multiple(self, register, length, reg_type):
        write_varuint(self.data, _REGISTER_READ_MULTIPLE_I8 + int(reg_type))
        write_varuint(self.data, register)
        write_varuint(self.data, length)

    def write_single(self, register, reg_value):
        write_varuint(self.data, _REGISTER_WRITE_SINGLE_I8 + int(reg_value.reg_type))
        write_varuint(self.data, register)
        self.data.write(_TYPE_FORMAT[reg_value.reg_type].pack(reg_value.value))

    def write_multiple(self, start_register, reg_values):
        assert len(reg_values) > 0
        same_as_first = [x.reg_type == reg_values[0].reg_type for x in reg_values]
        assert all(same_as_first)

        write_varuint(self.data, _REGISTER_WRITE_MULTIPLE_I8 + reg_values[0].reg_type)
        write_varuint(self.data, start_register)
        write_varuint(self.data, len(reg_values))
        for value in reg_values:
            self.data.write(_TYPE_FORMAT[value.reg_type].pack(value.value))


class RegisterValue:
    def __init__(self, value_in, reg_type_in):
        self.value = value_in
        self.reg_type = reg_type_in

    def __repr__(self):
        return '{}({})'.format(self.value, self.reg_type)

    def __eq__(self, other):
        return self.value == other.value and self.reg_type == other.reg_type


class RegisterError:
    def __init__(self, error_in):
        self.error = error_in

    def __repr__(self):
        return 'ERR[{}]'.format(self.error)


async def ParseRegisterReply(data):
    '''Parses a reply from a register operation.'''
    # The resulting data is stored here.
    result = {}
    stream = stream_helpers.AsyncStream(io.BytesIO(data))

    async def _parse_subframe(stream):
        subframe_id = await read_varuint(stream)
        if subframe_id is None:
            return None

        this_result = {}
        if (subframe_id & ~0x03) == _REGISTER_REPLY_SINGLE_I8:
            this_reg = await read_varuint(stream)
            this_fmt = _TYPE_FORMAT[subframe_id & 0x03]
            size = this_fmt.size
            data = await stream.read(size)

            if this_reg is None or data is None:
                return None
            this_result[this_reg] = RegisterValue(
                this_fmt.unpack(data)[0], subframe_id & 0x03)
        elif (subframe_id & ~0x03) == _REGISTER_REPLY_MULTIPLE_I8:
            start_reg = await read_varuint(stream)
            number_of_reg = await read_varuint(stream)
            if start_reg is None or number_of_reg is None:
                return None
            this_fmt = _TYPE_FORMAT[subframe_id & 0x03]

            for i in range(number_of_reg):
                data = await stream.read(this_fmt.size)
                if data is None:
                    return None
                this_result[start_reg + i] = RegisterValue(
                    this_fmt.unpack(data)[0], subframe_id & 0x03)
        elif subframe_id == _REGISTER_WRITE_ERROR:
            this_reg = await read_varuint(stream)
            this_err = await read_varuint(stream)
            this_result[this_reg] = RegisterError(this_err)
        elif subframe_id == _REGISTER_READ_ERROR:
            this_reg = await read_varuint(stream)
            this_err = await read_varuint(stream)
            this_result[this_reg] = RegisterError(this_err)
        else:
            return None

        return this_result

    while True:
        this_result = await _parse_subframe(stream)
        if this_result is None:
            break
        result.update(this_result)
    return result
