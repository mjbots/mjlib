# Rationale #

This telemetry format is conceptually similar to Apache AVRO
https://avro.apache.org

It is a serialization format where data is comprised of both a
"schema" and "data".  When transmitted or saved to persistent storage,
the schema associated with any serialized data is co-located.  When
deserializing, either this same schema, or a derivative of it can be
used.

Most of the same comparisons between other serialization schemes can
be made as with AVRO itself: https://avro.apache.org/docs/current/
However, the following differences apply:

 - The data format is designed such that many structures can be
   defined to ensure memory layout compatibility with serialized
   structures on many common platforms.
   - little endian formats are chosen
   - fixed byte size integer representations are provided
 - A canonical binary schema representation is provided in addition to
   a JSON one.
 - Recursive objects are not supported.

# Types #

## Primitive Types ##

 - `null` - No value
 - `boolean` - false or true
 - `fixedint` - a signed or unsigned fixed integer
 - `fixeduint` - a signed or unsigned fixed integer
 - `varint` - a minimally encoded int
 - `varuint` - a minimally encoded uint
 - `float32` - a 32 bit floating point value
 - `float64` - a 64 bit floating point value
 - `bytes` - N bytes of unformatted data
 - `string` - a UTF-8 encoded string

## Complex Types ##

### Object ###

An object consists of:

 - `name`: The name of this structure
 - `aliases`: Zero or more alternate names
 - `fields`: A list of fields, each described below

The "fields" array consists of multiple field descriptions, each of
which describes one field of the object.  Each field consists of:

 - `name`: The name of this field
 - `type`: A full schema declaration
 - `default`: A default value for this field
 - `aliases`: Zero or more alternate names
 - `version`: A JSON number which indicates the semantic
   interpretation of one or more fields differs from objects with an
   earlier version number.  This can be used to handle schema
   evolution more complicated than what JSON and "aliases" renamed
   fields provides for.


### Enum ###

An enumeration represents a type that must be one of a fixed set of
possible values.  It has the following attributes:

 - `name`: The name of this enumeration
 - `aliases`: Zero or more alternate names
 - `type`: A full schema declaration: Only the following types are
   supported:
    - `fixedint`
    - `fixeduint`
    - `varint`
    - `varuint`
 - `symbols`: A list of unique strings, each member must meet the same
   requirements as for names
 - `default`: A default value for this enumeration

### Array ###

This represents a list of items.  It has the following attributes:

 - `items`: A full schema declaration that describes the type of each
   element in the array.

### Fixed Array ###

This also represents a list of items of fixed length.  It has the
following attributes.

 - `size`: The number of items in every instance of this array.
 - `items`: A full schema declaration that describes the type of each
   element in the array.

### Map ###

This represents a map of strings to a value.  It has the following attributes:

 - `values`: A full schema declaration that describes that type of
   each items value.

### Union ###

This represents a value which may be one of a number of different
types.  It has the following attributes:

 - `types`: A list of ordered types.

The same constraints for allowable types from AVRO apply as well.
Thus, a union may not contain more than one element of the same type
except for types that have names (e.g. object, enum).  Additionally,
it may not contain another union as a member.

### Timestamp ###

This uses an underlying fixedint(8) to represent microseconds since the
UNIX epoch.  January 1, 1970 00:00:00 UTC.

### Duration ###

This uses an underlying fixedint(8) to represent microseconds of
duration.


# Names #

All names must obey the following regex: `[A-Za-z_][A-Za-z0-9_]+`


# Binary serializations #

The following conventions are used for all binary serializations and
for primitive types, with all values encoded in little endian form.

 - `null`: Nothing
 - `boolean`: A single byte.  0 represents false, 1 represents true,
   anything else is malformed.
 - `fixedint` / `fixeduint`: These are encoded using the natural
   little endian layout.
 - `varuint`: This is encoded one byte at a time.  If the high bit is
   set, then more bytes follow.
   - Value: 0 / 0x00
   - Value: 127 / 0x7f
   - Value: 128 / 0x0081
   - Value: 256 / 0x0082
 - `varint`: This is first encoded using zig-zag encoding, then as a
   varuint.  This means that small absolute values have a small
   encoding.
   - Value: 0 / 0x00
   - Value: -1 / 0x01
   - Value: 1 / 0x02
   - Value: -2 / 0x03
   - Value: 2 / 0x04
   - Value: -64 / 0x7f
   - Value: 64 / 0x0081
 - `float32` / `float64`: These are encoded by the most common IEEE
   encodings in little endian form.
 - `bytes` / `string`: These are encoded by a `varuint` followed by
   that many bytes.


# Schema Binary Serialization #

The schema for a given type can be serialized in a compact binary form as follows:

## Type ##

The type is encoded as a `varuint` with the following mapping:

 - `final`: 0
   - NOTE: This is not an actual type, but used to terminate lists.
 - `null`: 1
 - `boolean`: 2
 - `fixedint`: 3
 - `fixeduint`: 4
 - `varint`: 5
 - `varuint`: 6
 - `float32` : 7
 - `float64` : 8
 - `bytes`: 9
 - `string`: 10
 - `object`: 16
 - `enum`: 17
 - `array`: 18
 - `fixedarray`: 19
 - `map`: 20
 - `union` : 21
 - `timestamp` : 22
 - `duration` : 23

The following additional attributes follow these given types:

 - `fixedint`
   - One byte describing the size of the two's complement signed
     integer in bytes.  Allowable values are 1, 2, 4, and 8.
 - `fixeduint`
   - One byte describing the size of the unsigned integer in bytes.
     Allowable values are 1, 2, 4, and 8.
 - `object`
   - `ObjectFlags` : `varuint`
   - optional flag specific data
   - Zero or more "Field" entries.  The final must be of type `final`
 - `enum`
   - `type` : A complete "type" encoding.
   - `nvalues` : `varuint`
   - `nvalues` copies of:
     - `value` : corresponding to `type`
     - `name` : `string`
 - `array`
   - `items` : A complete "type" encoding
 - `fixedarray`
   - `size`: `varuint`
   - `items` : A complete "type" encoding
 - `map`
   - `values`: A complete "type" encoding
 - `union`
   - `types`: One or more "type" encodings.  The final must be of type
     `final`.

## Object Additional Data ##

### Flags ###

The following flags are defined.

 * `version` - 1 << 0
   * This indicates that a `varuint` field is present in the optional
     data section.  It has the same meaning as `version` in the JSON
     schema representation.

### Field ###

This comprises one element of the `object` types additional data.

 - `FieldFlags` : `varuint`
 - `name` : `string`
 - `aliases`
   - `naliases` : `varuint`
   - `naliases` `string` entries
 - `type` : A complete "type" encoding
 - `default` : A union of [ null, `type` ] which optionally gives the
   default value for this field.

# Schema JSON Serialization #

Schemas may be exchanged using JSON.  The following representations
are used for each type:

 - `null` : `"null"`
 - `boolean` : `"boolean"`
 - `fixedint` : One of:
   - `"fixedint8"`
   - `"fixedint16"`
   - `"fixedint32"`
   - `"fixedint64"`
 - `fixeduint` : One of:
   - `"fixeduint8"`
   - `"fixeduint16"`
   - `"fixeduint32"`
   - `"fixeduint64"`
 - `varint` : `"varint"`
 - `varuint` : `"varuint"`
 - `float32` : `"float32"`
 - `float64` : `"float64"`
 - `bytes` : `"bytes"`
 - `string` : `"string"`
 - `object`
```
{
  "type" : "object",
  "name" : "MyName",
  "aliases" : ["MyOldName"],
  "fields" : [
    { "name" : "value1",  "type" : "boolean" },
    { "name" : "value2",  "type" : "fixedint32", "default" : 123 }
  ]
}
```
 - `enum`
```
{
  "type" : "enum",
  "name" : "MyEnumName",
  "aliases" : ["MyOldEnumName"],
  "type" : "varuint",
  "symbols" : ["enumval1", "enumval2"]
}
```
 - `array`
```
{
  "type" : "array",
  "name" : "MyArrayName",
  "aliases" : ["MyOldArrayName"],
  "items" : "string"
}
```
 - `fixedarray`
```
{
  "type" : "fixedarray",
  "name" : "MyArrayName",
  "aliases" : ["MyOldArrayName"],
  "size" : "varuint",
  "items" : "string"
}
```
 - `map`
```
{
  "type" : "map",
  "values" : "boolean"
}
```
 - `timestamp` : `"timestamp"`
 - `duration` : `"duration"`


# Data Binary Serialization #

Each of the fields is serialized as follows for binary purposes.

 - All primitive fields are serialized using their representation as
   described above.
 - `object`: Each field is serialized in order.
 - `enum`: A single serialization corresponding to the integer type in
   the schema.
 - `array`:
   - `nelements` : `varuint`
   - `nelements` copies of the item serialization
 - `fixedarray`:
   - `size` copies of the item serialization
 - `map`:
   - `nitems` : `varuint`
   - `nitems` copies of
     - `key` : `string`
     - `value` : The serialization of the value
 - `union`
   - `index` : `varuint`  The zero based offset describing which type is selected
   - The serialization of the appropriate type
 - `timestamp` / `duration` : fixedint64

# Data JSON Serialization #

The following types are serialized in JSON as follows:

 - `null` : null
 - `boolean` : `false` | `true`
 - `fixedint` / `fixeduint` / `varint` / `varuint` : A JSON number
 - `float32` / `float64` : A JSON number
 - `bytes` : A JSON string containing base64 encoded data
 - `string` : A JSON string
 - `object` : A JSON "object"
 - `enum` : A JSON string containing the text of the element
 - `array`: A JSON array
 - `fixedarray`: A JSON array
 - `map` : A JSON object
 - `union` : A JSON "value".  TODO(jpieper): How to encode names.

# Binary Data File Format #

An on-disk format is defined to store instances of these
serializations.  The file logically consists of one or more instances
of "records" written in order.  Each "record" has a consistent type
throughout the file.

## Uses ##

This on-disk format can be used in a number of ways:

### An object persistence mechanism ###

For this purpose, the file consists of a single instance of a single
record.  The record name is not useful.  This provides a compact
mechanism for storing data while providing for schema evolution.

### A collection of data elements ###

For this purpose, a single record type is included, but multiple
instances of that record are included.

### An ordered set of multiple records ###

In this configuration, one or more records are interleaved within the
file.  This can be used to store a trace or log of an application in
time, where each record describes the state of one particular
component or subcomponent.

## Format ##

### Header ###

 - `TLOG0003`
 - `HeaderFlags` : `varuint`
 - Optional header specific information
 - One or more "Block"s

### Block ###

 - `BlockType` : `varuint`
 - `size` : `varuint`

Where BlockType can take one of the following values:

 - `Schema` : 1
 - `Data` : 2
 - `Index` : 3
 - `CompressionDictionary` : 4
 - `SeekMarker` : 5

### Schema ###

 - `identifier` : `varuint`
 - `flags` : `varuint`
 - optional flag specific information
 - `name` : `string`
 - A binary type serialization

### Data ###

 - `identifier` : `varuint`
 - `flags` : `varuint`
 - optional flag specific information
 - A binary data serialization

The following flags result in additional data being appended to the
end of the record, which will be present in the order the flags are
defined here:
 * `previous_offset` - 1 << 0
   * The additional data is a `varuint` giving the distance from the
     most recent data block for this identifier to the current block.
 * `timestamp` - 1 << 1
   * The additional data is a `fixedint64` timestamp
 * `checksum` - 1 << 2
   * The additional data is a 4 byte crc32 of the entire block
     assuming the CRC field is all 00s.

The following flags do not result in additional data being appended.
 * `snappy` - 1 << 4
   * The following binary serialization has been compressed with the
     "snappy" compression algorithm.

### Index ###

 * `flags` - varuint
 * `nelements` - varuint
 * `nelements` copies of
   * `identifier` - `varuint`
   * `schemalocation` - fixeduint64
     * The location in this file where the schema record can be found
   * `finalrecord` - fixeduint64
     * The location in this file where the final record can be found
 * `size` - fixeduint32
   * The size of the index record
 * "TLOGIDEX" - a constant 8 byte string

### CompressionDictionary ###

TODO

### SeekMarker ###

 * `marker` - uint64_t
   * A fixed `0xfdcab9a897867564`
 * `crc32` - uint32_t
   * The CRC32 of this entire block, assuming the CRC field is all 0s
 * `header_len` - uint8_t
   * The length of the block type and block size records
 * `flags` - varuint
 * `timestamp` - fixedint64
 * `nelements` - `varuint`
 * `nelements` copies of
   * `identifier` - varuint
   * `previous_offset` - varuint
 * optional data

Possible flags:

* `compression_dictionary` - A `uint64` will be included in the
  optional data which provides the location of the most recent
  `CompressionDictionary` block.

# Websocket #

A websocket based protocol is defined for clients to monitor the state
of a system that produces a hierarchical set of formatted structures.

The available topics form a hierarchy, with levels separated by
forward slashes ('/').  Each topic has a fixed schema, and provides
data updates at regular or possibly irregular intervals.

It is expected to use the websocket endpoint "/telemetry" with text
frames containing JSON structures.  Each structure contains the field
"command", wich may be one of the following:

 - `subscribe`
 - `unsubscribe`
 - `publish_start`
 - `publish`
 - `publish_stop`
 - `error`

## `subscribe` ##

The `subscribe` command is a message from the client to the server
requesting to receive updates on specific topics when they are
available.  The following fields are current defined.

 * `topic` - a string to describe which topics are of interest.  It
   supports the '*' wildcard which does not match the directory
   separator, and the '**' wildcard, which matches zero or more
   complete path segments, but can only be used as a complete path
   segment.
 * `id` - a client assigned identifier used to refer to this
   subscription in the future.  The client must ensure that
   outstanding subscriptions have unique identifiers.
 * `schema_only` - if present, and set to True, then no `publish`
   messages will be sent for this subscription

```
{
  "command" : "subscribe",
  "topic" : "/my/first/topic",
  "id" : "12abdcef"
}
```

## `unsubscribe` ##

The `unsubscribe` command is a message from the client to the server
requesting to stop the given subscription.  A `publish_stop` message
will be sent from the server to the client for any topics which have
an outstanding `publish_start`.

Fields:

 * `id` - an identifier previously sent in a `subscribe` message and
   not yet sent via `unsubscribe`.

```
{
  "command" : "unsubscribe",
  "id" : "12abdcef"
}
```


## `publish_start` ##

The `publish_start` is sent from the server to the client to indicate
that the given message is available and may have `publish` messages
sent for it.

Fields:

 * `subscribe_id` - the client assigned identifier for the `subscribe`
   message that initiated this publish_start
 * `publish_id` - a server assigned identifier unique to this stream
   of data
 * `topic` - the topic string
 * `schema` - a base64 encoded binary schema representation

```
{
  "command" : "publish_start",
  "subscribe_id" : "12abdcef",
  "publish_id" : "bvcdef",
  "topic" : "/my/first/topic",
  "schema" : "YWJjZGVm"
}
```


## `publish` ##

The `publish` message is sent from the server to the client to
indicate that new data is available for the given topic.

Fields:

 * `id` - the unique identifier that was sent with the `publish_start` message
 * `data` - a base64 encoded binary data representation

```
{
  "command" : "publish",
  "id" : "bvcdef",
  "data" : "QUFBQQ==",
}
```

## `publish_stop` ##

The `publish_stop` message is sent from the server to the client to
indicate that the given topic is no longer available.

Fields:

 * `id` - the unique identifier that was sent with the `publish_start`
   message

```
{
  "command" : "publish_stop",
  "id" : "bvcdef",
}
```
