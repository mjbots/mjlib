# Copyright 2020 Josh Pieper, jjp@pobox.com.
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

import enum
import io
import snappy

import mjlib.telemetry.reader as reader


_HEADER = b'TLOG0003'


class DataFlags(enum.IntEnum):
    previous_offset = 1 << 0
    timestamp = 1 << 1
    checksum = 1 << 2
    snappy = 1 << 4


class BlockType(enum.IntEnum):
    Schema = 1
    Data = 2
    Index = 3
    CompressionDictionary = 4
    SeekMarker = 5


class FileReader:
    '''Provides mechanisms to read and seek in a log file written using
    the format described in README.md'''

    def __init__(self, filename):
        self._records = {}

        # Open, look for the header.
        if type(filename) == str:
            self._fd = open(filename, 'rb')
        else:
            self._fd = filename
        self._fd.seek(0, 0)
        header = self._fd.read(8)

        assert header == _HEADER

    class Block:
        btype = None
        position = None
        data = None

    def _read_blocks(self, start=None, end=None):
        '''Iterate over all blocks, yielding their data.'''
        self._fd.seek(start if start else len(_HEADER), 0)

        stream = reader.Stream(self._fd)
        file_flags = stream.read_varuint()
        assert file_flags == 0

        while True:
            result = FileReader.Block()

            result.position = self._fd.tell()
            if end is not None and result.position >= end:
                return

            try:
                result.btype = stream.read_varuint()
                block_size = stream.read_varuint()
            except EOFError:
                break

            result.data = self._fd.read(block_size)
            if len(result.data) != block_size:
                # An incomplete read, we'll call that done.
                break

            yield result

    class Schema:
        identifier = None
        flags = None

        name = None

        # An instance of reader.Type
        reader = None

        serialized_schema = None


    def _parse_schema(self, block_data):
        result = FileReader.Schema()

        raw_stream = io.BytesIO(block_data)
        stream = reader.Stream(raw_stream)
        result.identifier = stream.read_varuint()
        result.flags = stream.read_varuint()
        assert result.flags == 0
        result.name = stream.read_string()
        result.serialized_schema = raw_stream.read()

        result.reader = reader.Type.from_binary(
            io.BytesIO(result.serialized_schema))

        return result

    class Item:
        identifier = None
        flags = None

        # A token that can be used as an argument to 'start' or 'end'
        position = None

        # The parsed data, if available.
        data = None

        # The serialized data.
        serialized_data = None

        # A reference to a Schema instance
        schema = None


    def _parse_data(self, id_set, block_data):
        result = FileReader.Item()

        raw_stream = io.BytesIO(block_data)
        stream = reader.Stream(raw_stream)

        result.identifier = stream.read_varuint()
        if id_set is not None:
            if result.identifier not in id_set:
                return None

        result.flags = stream.read_varuint()
        flags = result.flags

        if not result.identifier in self._records:
            return None

        result.schema = self._records[result.identifier]

        if flags & DataFlags.previous_offset:
            flags &= ~(DataFlags.previous_offset)
            _ = stream.read_varuint()
        if flags & DataFlags.timestamp:
            flags &= ~(DataFlags.timestamp)
            result.timestamp = stream.read_i64() / 1000000.0
            # TODO: turn this into a more usable type.
        if flags & DataFlags.checksum:
            flags &= ~(DataFlags.checksum)
            checksum = stream.read_u32()  # ignore for now

        result.serialized_data = raw_stream.read()

        if flags & DataFlags.snappy:
            flags &= ~(DataFlags.snappy)
            result.serialized_data = snappy.uncompress(result.serialized_data)

        result.data = result.schema.reader.read(
            reader.Stream(io.BytesIO(result.serialized_data)))

        assert flags == 0  # no unknown flags

        return result


    def items(self, records=[], start=None, end=None):
        # Iterate over items in the log, optionally constrained by a
        # set of records, and a start and end token.  Each returned
        # item is an 'Item' structure.

        id_set = set() if records else None

        for block in self._read_blocks(
                start=start, end=end):
            if block.btype == BlockType.Schema:
                record = self._parse_schema(block.data)
                self._records[record.identifier] = record
                if record.name in records and id_set is not None:
                    id_set.add(record.identifier)
            elif block.btype == BlockType.Data:
                item = self._parse_data(id_set, block.data)
                if item is None:
                    continue
                yield item

    def get(self, records=[]):
        # A convenience interface which reads the entirety of a log
        # into memory, limited to a set of records and returns it as a
        # giant dict of arrays.

        result = {}
        for item in self.items(records):
            name = item.schema.name
            if name not in result:
                result[name] = []
            result[name].append(item)

        return result


    def records(self):
        result = {}

        for block in self._read_blocks():
            if block.btype != BlockType.Schema:
                continue

            schema = self._parse_schema(block.data)
            result[schema.name] = schema.reader

        return result
