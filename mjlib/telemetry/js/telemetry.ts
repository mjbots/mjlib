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


/**
 * Given a buffer, provide methods to consume a telemetry formatted
 * bytestream.
 */
export class ReadStream {
  pos: number = 0;

  constructor(public buf : Buffer) {
  }

  ignore(size: number) {
    this.pos += size;
    if (this.pos >= this.buf.length) {
      throw Error("eof");
    }
  }

  readVaruint() : number {
    var result : number = 0;
    var fk : number = 1;
    var value : number = 0;
    var buf = this.buf;

    do {
      if (this.pos >= this.buf.length) {
        throw Error("eof");
      }
      value = buf[this.pos++];
      result = result + fk * (value & 0x7f);
      fk = fk * 128;
      // TODO jpieper: Handle malformed values that overflow a 64 bit
      // value.
    } while (value >= 0x80);

    return result;
  }

  readString() : string {
    var size = this.readVaruint();
    var start = this.advance(Number(size));
    return this.buf.toString('utf8', start, Number(size));
  }

  readBytes() : Buffer {
    var size = this.readVaruint();
    var start = this.advance(Number(size));
    return Buffer.from(this.buf, start, start + Number(size));
  }

  readf32() : number {
    return this.buf.readFloatLE(this.advance(4));
  }

  readf64() : number {
    return this.buf.readDoubleLE(this.advance(8));
  }

  readu8() : number {
    return this.buf.readUInt8(this.advance(1));
  }

  readu16() : number {
    return this.buf.readUInt16LE(this.advance(2));
  }

  readu32() : number {
    return this.buf.readUInt32LE(this.advance(4));
  }

  readu64() : number {
    return this.readu32() + this.readu32() * 4294967296;
  }

  readi8() : number {
    return this.buf.readInt8(this.advance(1));
  }

  readi16() : number {
    return this.buf.readInt16LE(this.advance(2));
  }

  readi32() : number {
    return this.buf.readInt32LE(this.advance(4));
  }

  readi64() : number {
    var unsigned = this.readu64();
    // TODO jpieper: Implement me!
    return unsigned;
  }

  private advance(offset : number): number {
    var start = this.pos;
    this.pos += offset;
    if (this.pos > this.buf.length) {
      throw Error("eof");
    }
    return start;
  }
}
