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
    return this.buf.toString('utf8', start, start + Number(size));
  }

  readBytes() : Buffer {
    var size = this.readVaruint();
    var start = this.advance(Number(size));
    return this.buf.slice(start, start + Number(size));
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

abstract class Type {
  abstract read(dataStream: ReadStream) : any;
}

class FinalType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(dataStream: ReadStream) : any {
    throw Error("invalid");
  }
}

class NullType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(dataStream: ReadStream) : any {
    return null;
  }
}

class BooleanType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return !!stream.readu8();
  }
}

class FixedIntType implements Type {
  fieldSize : number;

  constructor(schemaStream: ReadStream) {
    this.fieldSize = schemaStream.readu8();
    if (this.fieldSize != 1 &&
        this.fieldSize != 2 &&
        this.fieldSize != 4 &&
        this.fieldSize != 8) {
      throw Error("invalid fixedint size")
    }
  }

  read(stream: ReadStream) : any {
    switch (this.fieldSize) {
    case 1:
      return stream.readi8();
    case 2:
      return stream.readi16();
    case 4:
      return stream.readi32();
    case 8:
      return stream.readi64();
    default:
      throw Error("unreachable");
    }
  }
}

class FixedUIntType implements Type {
  fieldSize : number;

  constructor(schemaStream: ReadStream) {
    this.fieldSize = schemaStream.readu8();
    if (this.fieldSize != 1 &&
        this.fieldSize != 2 &&
        this.fieldSize != 4 &&
        this.fieldSize != 8) {
      throw Error("invalid fixedint size")
    }
  }

  read(stream: ReadStream) : any {
    switch (this.fieldSize) {
    case 1:
      return stream.readu8();
    case 2:
      return stream.readu16();
    case 4:
      return stream.readu32();
    case 8:
      return stream.readu64();
    default:
      throw Error("unreachable");
    }
  }
}

class VarintType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    throw Error("not implemented");
  }
}

class VaruintType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return stream.readVaruint();
  }
}

class Float32Type implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return stream.readf32();
  }
}

class Float64Type implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return stream.readf64();
  }
}

class BytesType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return stream.readBytes();
  }
}

class StringType implements Type {
  constructor(schemaStream: ReadStream) {}

  read(stream: ReadStream) : any {
    return stream.readString();
  }
}

class Field {
  constructor(public flags : Number,
              public name : string,
              public aliases : string[],
              public type : Type,
              public defaultValue : any) {}
}

class ObjectType implements Type {
  flags : number = 0;
  fields : Field[] = [];

  constructor(schemaStream: ReadStream) {
    this.flags = schemaStream.readVaruint();

    do {
      var flags = schemaStream.readVaruint();
      var name = schemaStream.readString();
      var naliases = schemaStream.readVaruint();
      var aliases : string[] = [];
      for (var i = 0; i < naliases; i++) {
        aliases.push(schemaStream.readString());
      }
      var type = createBinaryType(schemaStream);
      var isDefault = !!schemaStream.readu8();
      var defaultValue = isDefault ? type.read(schemaStream) : null;
      if (type instanceof FinalType) {
        break;
      }
      this.fields.push(new Field(flags, name, aliases, type, defaultValue));
    } while(true);
  }

  read(stream: ReadStream) : any {
    var result : { [key: string]: any} = {};
    for (let field of this.fields) {
      result[field.name] = field.type.read(stream);
    }
    return result;
  }
}

var TYPES  = [
  FinalType,      // 0
  NullType,       // 1
  BooleanType,    // 2
  FixedIntType,   // 3
  FixedUIntType,  // 4
  VarintType,     // 5
  VaruintType,    // 6
  Float32Type,    // 7
  Float64Type,    // 8
  BytesType,      // 9
  StringType,     // 10
  undefined,      // 11
  undefined,      // 12
  undefined,      // 13
  undefined,      // 14
  undefined,      // 15
  ObjectType,     // 16
];


/***
 * Given a binary schema, return a type which can convert binary data
 * into native javascript objects.
 */
export function createBinaryType(schemaStream: ReadStream) : Type {
  var typeIndex = schemaStream.readVaruint();
  var thisType = TYPES[typeIndex];
  if (thisType === undefined) {
    throw Error(`Unknown type ${typeIndex}`);
  }
  return new thisType(schemaStream);
}
