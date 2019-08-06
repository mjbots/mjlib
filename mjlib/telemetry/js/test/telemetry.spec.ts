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
