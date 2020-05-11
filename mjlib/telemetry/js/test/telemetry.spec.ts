/**
 *  Copyright 2019 Josh Pieper, jjp@pobox.com.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import * as telemetry from "../telemetry"

describe('ReadStream', () => {
  it('construction', () => {
    var dut = new telemetry.ReadStream(Buffer.from([]));
    expect(dut).toBeDefined();
  });

  it('ignore', () => {
    var dut = new telemetry.ReadStream(Buffer.from([0x01, 0x02, 0x03]));
    dut.ignore(1);
    expect(dut.pos).toBe(1);
    expect(function() { dut.ignore(3);}).toThrow(Error("eof"));
  });

  it('varuint', () => {
    var dut = new telemetry.ReadStream(Buffer.from([0x00, 0x01, 0x80, 0x01]));
    expect(dut.readVaruint()).toBe(0);
    expect(dut.readVaruint()).toBe(1);
    expect(dut.readVaruint()).toBe(128);
  });

  // it ('varuint_big', () => {
  //   var dut = new telemetry.ReadStream(Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01]));
  //   expect(dut.readVaruint()).toBe(BigInt("18446744073709551615"));
  // });

  it('ints', () => {
    expect((new telemetry.ReadStream(Buffer.from([0x08]))).readu8()).toBe(8);
    expect((new telemetry.ReadStream(Buffer.from([0x08, 0x01]))).readu16()).toBe(256 + 8);
    expect((new telemetry.ReadStream(Buffer.from([0x08, 0x00, 0x00, 0x01]))).readu32()).toBe(16777216 + 8);
    // expect((new telemetry.ReadStream(Buffer.from([0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02]))).readu64()).toBe(BigInt("0x0200000100000008"));
  });
});

describe('createBinaryType', () => {
  function makeBinaryType(data : Number[]) {
    return telemetry.Type.fromBinary(new telemetry.ReadStream(Buffer.from(data)));
  }

  function readBinary(schema: Number[], data: Number[]) {
    var t = makeBinaryType(schema);
    var d = new telemetry.ReadStream(Buffer.from(data));
    return t.read(d);
  }

  it('null', () => {
    expect(readBinary([0x01], [])).toBe(null);
  });

  it('bool', () => {
    expect(readBinary([0x02], [0x01])).toBe(true);
  });

  it('fixedint8', () => {
    expect(readBinary([0x03, 0x01], [0xff])).toBe(-1);
  });

  it('fixedint16', () => {
    expect(readBinary([0x03, 0x02], [0x04, 0x01])).toBe(256 + 4);
  });

  it('fixedint32', () => {
    expect(readBinary([0x03, 0x04], [0x04, 0x00, 0x00, 0x01]))
      .toBe(16777216 + 4);
  });

  it('fixedint64', () => {
    expect(readBinary(
      [0x03, 0x08],
      [0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00]))
      .toBe(4294967296 + 4);
  });

  it('fixeduint8', () => {
    expect(readBinary([0x04, 0x01], [0xff])).toBe(255);
  });

  it('fixeduint16', () => {
    expect(readBinary([0x04, 0x02], [0xfe, 0xff])).toBe(65534);
  });

  it('fixeduint32', () => {
    expect(readBinary([0x04, 0x04], [0xfd, 0xff, 0xff, 0xff]))
      .toBe(4294967296 - 3);
  });

  it('fixeduint64', () => {
    expect(readBinary([0x04, 0x08], [0xfd, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00]))
      .toBe(4294967296 * 2 - 3);
  });

  it('varuint', () => {
    expect(readBinary([0x06], [0x81, 0x01])).toBe(129);
  });

  it('float32', () => {
    expect(readBinary([0x07], [0x00, 0x00, 0x00, 0x00])).toBe(0.0);
  });

  it('float64', () => {
    expect(readBinary(
      [0x08], [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]))
      .toBe(0.0);
  });

  it('bytes', () => {
    var result = readBinary([0x09], [0x03, 0x10, 0x11, 0x12]);
    expect(result.length).toBe(3);
    expect(result[0]).toBe(0x10);
    expect(result[1]).toBe(0x11);
    expect(result[2]).toBe(0x12);
  });

  it('string', () => {
    expect(readBinary([0x0a], [0x03, 0x64, 0x65, 0x66])).toBe("def");
  });

  it('object', () => {
    var result = readBinary([
      0x10,  // "object"
      0x00,  // flags
       0x00,  // FieldFlags
       0x04, 0x74, 0x65, 0x73, 0x74,  // "test"
       0x00,  // naliases=0
       0x02,  // "boolean"
       0x01, 0x00,  // default: false

       0x00,  // FieldFlags
       0x04, 0x74, 0x65, 0x73, 0x32,  // "tes2"
       0x00,  // naliases=0
       0x06,  // "varuint"
       0x01, 0x05,  // default: 5

       0x00,  // FieldFlags
       0x00,
       0x00,  // naliases=0
       0x00,  // "final"
       0x00,  // no default
    ], [0x01, 0x05]);
    expect(result.test).toBe(true);
    expect(result.tes2).toBe(5);
  });

  it('enum', () => {
    var result = readBinary([
      0x11,  // "enum"
       0x06,  // type: "varuint"
       0x03,  // nvalues
        0x02, 0x03, 0x76, 0x6c, 0x31,  // 2 = "vl1"
        0x05, 0x02, 0x76, 0x32, // 5 = "v2"
        0x09, 0x03, 0x76, 0x6c, 0x33,  // 9 = "vl3"
    ], [0x05]);
    expect(result).toBe("v2");
  });

  it('array', () => {
    var result = readBinary([
      0x12,
      0x04, 0x01,
    ], [0x02, 0x04, 0x09]);
    expect(result.length).toBe(2);
    expect(result[0]).toBe(4);
    expect(result[1]).toBe(9);
  });

  it('fixedarray', () => {
    var result = readBinary([
      0x13,
      0x02,
      0x04, 0x01,
    ], [0x02, 0x09]);
    expect(result.length).toBe(2);
    expect(result[0]).toBe(2);
    expect(result[1]).toBe(9);
  });

  it('map', () => {
    var result = readBinary([
      0x14,
      0x04, 0x01,
    ], [
      0x02,
      0x02, 0x76, 0x31,   0x06,  // v1 : 6
      0x03, 0x76, 0x61, 0x32,  0x08,  // va2 : 8
    ]);
    expect(result.v1).toBe(6);
    expect(result.va2).toBe(8);
  });

  it('union', () => {
    var result = readBinary([
      0x15,
      0x01,
      0x02,
      0x03, 0x02,
      0x00,
    ], [0x01, 0x01]);
    expect(result).toBe(true);
  });

  it('timestamp', () => {
    var result = readBinary([0x16], [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00]);
    expect(result.toISOString()).toBe("1978-12-02T19:29:36.710Z");
  });

  it('duration', () => {
    var result = readBinary([0x17], [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00]);
    expect(result).toBe(281474976.710656);
  });
});
