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


import io
import unittest


import mjlib.telemetry.file_reader as file_reader


_SAMPLE_LOG = (
    b'TLOG0003' +
    bytes([
        0x00,  # log flags

        0x01, 0x08,  # BlockType - Schema
        0x01, 0x00,  # id=1, flags=0
        0x04, ord('t'), ord('e'), ord('s'), ord('t'),
        0x02,  # boolean

        0x02, 0x0c,  # BlockType - Data
        0x01, 0x03,  # id=1, flags = (previous_offset|timestamp)
        0x00,  # previous_offset
        0x00, 0x20, 0x07, 0xcd, 0x74, 0xa, 0x05, 0x00,  # timestamp
        0x01,  # true

        0x03, 0x1f,  # BlockType - Index, size=31
        0x00, 0x01,  # flags=0, nelements=1
          0x01,  # id
            0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # schema loc
            0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # final rec
          0x21, 0x00, 0x00, 0x00,  # final size
        ord('T'), ord('L'), ord('O'), ord('G'),
        ord('I'), ord('D'), ord('E'), ord('X'),
    ]))

_COMPRESSED_LOG = (
    bytes([
        ord('T'), ord('L'), ord('O'), ord('G'),
        ord('0'), ord('0'), ord('0'), ord('3'), 0,  # file header
        0x01, 0x08,  # BlockType - Schema, size
        0x01, 0x00,  # id=1, flags = 0
        0x04, ord('t'), ord('e'), ord('s'), ord('t'),  # name
        0x0a,  # schema

        0x02, 0x43,  # BlockType = Data, size
        0x01, 0x17,  # id=1, flags= (previous_offset|timestamp|checksum|zstd)
        0x00,  # previous offset
        0x00, 0x20, 0x07, 0xcd, 0x74, 0xa0, 0x05, 0x00,  # timestamp
        0x1a, 0x6e, 0x76, 0x9e,  # crc32

        0x80, 0x08, 0x00, 0x61, 0xfe, 0x01, 0x00, 0xfe,
        0x01, 0x00, 0xfe, 0x01, 0x00, 0xfe, 0x01, 0x00,
        0xfe, 0x01, 0x00, 0xfe, 0x01, 0x00, 0xfe, 0x01,
        0x00, 0xfe, 0x01, 0x00, 0xfe, 0x01, 0x00, 0xfe,
        0x01, 0x00, 0xfe, 0x01, 0x00, 0xfe, 0x01, 0x00,
        0xfe, 0x01, 0x00, 0xfe, 0x01, 0x00, 0xfe, 0x01,
        0x00, 0xfa, 0x01, 0x00,

        0x03, 0x1f,  # BlockType = Index, size=31
        0x00, 0x01,  # flags=0 nelements=1
         0x01, # id
          0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # schema location
          0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # final record
          0x21, 0x00, 0x00, 0x00,
        ord('T'), ord('L'), ord('O'), ord('G'),
        ord('I'), ord('D'), ord('E'), ord('X'),

    ])
)

class FileReaderTest(unittest.TestCase):
    def test_basic(self):
        dut = file_reader.FileReader(io.BytesIO(_SAMPLE_LOG))
        everything = dut.get()
        self.assertEqual(list(everything.keys()), ["test"])
        datalist = everything["test"]
        self.assertEqual(len(datalist), 1)
        self.assertEqual(datalist[0].data, True)

    def test_compressed(self):
        dut = file_reader.FileReader(io.BytesIO(_COMPRESSED_LOG))
        everything = dut.get()
        self.assertEqual(list(everything.keys()), ["test"])
        datalist = everything["test"]
        self.assertEqual(len(datalist), 1)
        # The actual data actually uncompressed to 1024 a's, however
        # we have encoded it as a string, so the first character ends
        # up denoting the string length.
        self.assertEqual(datalist[0].data, 'a' * ord('a'))


if __name__ == '__main__':
    unittest.main()
