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

export abstract class Type {
  abstract read(dataStream: ReadStream) : any;

  /***
   * Given a binary schema, return a type which can convert binary data
   * into native javascript objects.
   */
  static fromBinary(schemaStream: ReadStream) : Type {
    var typeIndex = schemaStream.readVaruint();
    var thisType = TypesFromBinary[typeIndex];
    if (thisType === undefined) {
      throw Error(`Unknown type ${typeIndex}`);
    }
    return thisType(schemaStream);
  }
}

export class FinalType implements Type {
  static fromBinary(schemaStream: ReadStream) : FinalType { return new FinalType(); }

  read(dataStream: ReadStream) : any {
    throw Error("invalid");
  }
}

export class NullType implements Type {
  static fromBinary(schemaStream: ReadStream) : NullType { return new NullType(); }

  read(dataStream: ReadStream) : any {
    return null;
  }
}

export class BooleanType implements Type {
  static fromBinary(schemaStream: ReadStream) : BooleanType { return new BooleanType(); }

  read(stream: ReadStream) : any {
    return !!stream.readu8();
  }
}

export class FixedIntType implements Type {
  static fromBinary(schemaStream: ReadStream) : FixedIntType {
    return new FixedIntType(schemaStream.readu8());
  }

  constructor(public fieldSize : number) {
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

export class FixedUIntType implements Type {
  static fromBinary(schemaStream: ReadStream) : FixedUIntType {
    return new FixedUIntType(schemaStream.readu8());
  }

  constructor(public fieldSize : number) {
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

export class VarintType implements Type {
  static fromBinary(schemaStream: ReadStream) : VarintType {
    return new VarintType();
  }

  read(stream: ReadStream) : any {
    throw Error("not implemented");
  }
}

export class VaruintType implements Type {
  static fromBinary(schemaStream: ReadStream) : VaruintType {
    return new VaruintType();
  }

  read(stream: ReadStream) : any {
    return stream.readVaruint();
  }
}

export class Float32Type implements Type {
  static fromBinary(schemaStream: ReadStream) : Float32Type {
    return new Float32Type();
  }

  read(stream: ReadStream) : any {
    return stream.readf32();
  }
}

export class Float64Type implements Type {
  static fromBinary(schemaStream: ReadStream) : Float64Type {
    return new Float64Type();
  }

  read(stream: ReadStream) : any {
    return stream.readf64();
  }
}

export class BytesType implements Type {
  static fromBinary(schemaStream: ReadStream) : BytesType {
    return new BytesType();
  }

  read(stream: ReadStream) : any {
    return stream.readBytes();
  }
}

export class StringType implements Type {
  static fromBinary(schemaStream: ReadStream) : StringType {
    return new StringType();
  }

  read(stream: ReadStream) : any {
    return stream.readString();
  }
}

export class Field {
  constructor(public flags : Number,
              public name : string,
              public aliases : string[],
              public type : Type,
              public defaultValue : any) {}
}

export class ObjectType implements Type {
  flags : number = 0;
  fields : Field[] = [];

  static fromBinary(schemaStream: ReadStream) : ObjectType {
    var result = new ObjectType();

    result.flags = schemaStream.readVaruint();

    do {
      var flags = schemaStream.readVaruint();
      var name = schemaStream.readString();
      var naliases = schemaStream.readVaruint();
      var aliases : string[] = [];
      for (var i = 0; i < naliases; i++) {
        aliases.push(schemaStream.readString());
      }
      var type = Type.fromBinary(schemaStream);
      var isDefault = !!schemaStream.readu8();
      var defaultValue = isDefault ? type.read(schemaStream) : null;
      if (type instanceof FinalType) {
        break;
      }
      result.fields.push(new Field(flags, name, aliases, type, defaultValue));
    } while(true);

    return result;
  }

  read(stream: ReadStream) : any {
    var result : { [key: string]: any} = {};
    for (let field of this.fields) {
      result[field.name] = field.type.read(stream);
    }
    return result;
  }
}

export class EnumType implements Type {
  type: Type;
  items : string[] = [];

  static fromBinary(schemaStream: ReadStream) : EnumType {
    var result = new EnumType();
    result.type = Type.fromBinary(schemaStream);
    var nvalues = schemaStream.readVaruint();
    for (var i = 0; i < nvalues; i++) {
      var key = schemaStream.readVaruint();
      var name = schemaStream.readString();
      result.items[key] = name;
    }
    return result;
  }

  read(stream: ReadStream) : any {
    var key = this.type.read(stream);
    return this.items[key];
  }
}

export class ArrayType implements Type {
  type: Type;

  static fromBinary(schemaStream: ReadStream) : ArrayType {
    var result = new ArrayType();
    result.type = Type.fromBinary(schemaStream);
    return result;
  }

  read(stream: ReadStream) : any {
    var result : any[] = [];

    var nvalues = stream.readVaruint();
    for (var i = 0; i < nvalues; i++) {
      result.push(this.type.read(stream));
    }
    return result;
  }
}

export class FixedArrayType implements Type {
  size: number;
  type: Type;

  static fromBinary(schemaStream: ReadStream) : FixedArrayType {
    var result = new FixedArrayType();
    result.size = schemaStream.readVaruint();
    result.type = Type.fromBinary(schemaStream);
    return result;
  }

  read(stream: ReadStream) : any {
    var result : any[] = [];

    for (var i = 0; i < this.size; i++) {
      result.push(this.type.read(stream));
    }
    return result;
  }
}

export class MapType implements Type {
  type: Type;

  static fromBinary(schemaStream: ReadStream) : MapType {
    var result = new MapType();
    result.type = Type.fromBinary(schemaStream);
    return result;
  }

  read(stream: ReadStream) : any {
    var result : { [key: string]: any } = {};
    var nitems = stream.readVaruint();
    for (var i = 0; i < nitems; i++) {
      var key = stream.readString();
      var value = this.type.read(stream);
      result[key] = value;
    }
    return result;
  }
}

export class UnionType implements Type {
  type: Type[] = [];

  static fromBinary(schemaStream: ReadStream) : UnionType {
    var result = new UnionType();
    do {
      var thisType = Type.fromBinary(schemaStream);
      if (thisType instanceof FinalType) { break; }
      result.type.push(thisType);
    } while (true);
    return result;
  }

  read(stream: ReadStream) : any {
    var index = stream.readVaruint();
    return this.type[index].read(stream);
  }
}

export class TimestampType implements Type {
  static fromBinary(schemaStream: ReadStream) : TimestampType {
    return new TimestampType();
  }

  read(stream: ReadStream) : any {
    var usSinceEpoch = stream.readi64();
    var result : any = new Date(usSinceEpoch / 1000);
    result['valueUs'] = usSinceEpoch;
    return result;
  }
}

export class DurationType implements Type {
  static fromBinary(schemaStream: ReadStream) : DurationType {
    return new DurationType();
  }

  read(stream: ReadStream) : any {
    var us = stream.readi64();
    // For now, represent this as floating point seconds.
    return us / 1000000.0;
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
  EnumType,       // 17
  ArrayType,      // 18
  FixedArrayType, // 19
  MapType,        // 20
  UnionType,      // 21
  TimestampType,  // 22
  DurationType,   // 23
];

var TypesFromBinary = TYPES.map(
  (type : any) => (type === undefined) ? undefined : type.fromBinary)
