# -*- python -*-

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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "format",
    hdrs = ["format.h"],
)

cc_library(
    name = "stream",
    hdrs = ["stream.h"],
    deps = [
        ":format",
        "//mjlib/base:assert",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:stream",
    ],
)

cc_library(
    name = "register",
    hdrs = ["register.h"],
    srcs = ["register.cc"],
    deps = [
        ":format",
        ":stream",
        "//mjlib/base:assert",
        "//mjlib/base:fail",
        "//mjlib/base:fast_stream",
        "//mjlib/base:stream",
    ],
)

cc_library(
    name = "frame",
    hdrs = ["frame.h"],
    srcs = ["frame.cc"],
    deps = [
        ":format",
        ":stream",
        "//mjlib/base:crc_stream",
        "//mjlib/base:fast_stream",
        "@boost",
    ],
)

cc_library(
    name = "frame_stream",
    hdrs = [
        "fdcanusb_frame_stream.h",
        "frame_stream.h",
        "rs485_frame_stream.h",
        "socketcan_frame_stream.h",
    ],
    srcs = [
        "fdcanusb_frame_stream.cc",
        "rs485_frame_stream.cc",
        "socketcan_frame_stream.cc",
    ],
    deps = [
        ":format",
        ":frame",
        ":stream",
        "//mjlib/base:crc_stream",
        "//mjlib/base:fail",
        "//mjlib/base:tokenizer",
        "//mjlib/io:async_stream",
        "//mjlib/io:debug_time",
        "//mjlib/io:streambuf_read_stream",
        "//mjlib/io:stream_factory",
        "@boost",
        "@fmt",
    ],
)

cc_library(
    name = "asio_client",
    hdrs = ["asio_client.h"],
    deps = [
        ":register",
        "//mjlib/io:async_stream",
    ],
)

cc_library(
    name = "stream_asio_client",
    hdrs = ["stream_asio_client.h"],
    srcs = ["stream_asio_client.cc"],
    deps = [
        ":asio_client",
        ":frame_stream",
        ":register",
        ":stream",
        "//mjlib/base:error_code",
        "//mjlib/base:fast_stream",
        "//mjlib/io:async_stream",
        "//mjlib/io:exclusive_command",
        "//mjlib/io:offset_buffer",
    ],
)

cc_library(
    name = "stream_asio_client_builder",
    hdrs = ["stream_asio_client_builder.h"],
    deps = [
        ":frame_stream",
        ":stream_asio_client",
        "//mjlib/io:selector",
    ],
)

cc_library(
    name = "micro_error",
    hdrs = ["micro_error.h"],
    srcs = ["micro_error.cc"],
    deps = [
        "//mjlib/micro:error_code",
    ],
)

cc_library(
    name = "micro_datagram_server",
    hdrs = ["micro_datagram_server.h"],
    deps = [
        "//mjlib/base:string_span",
        "//mjlib/micro:async_types",
    ],
)

cc_library(
    name = "micro_stream_datagram",
    hdrs = ["micro_stream_datagram.h"],
    srcs = ["micro_stream_datagram.cc"],
    deps = [
        ":format",
        ":micro_datagram_server",
        ":micro_error",
        ":stream",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:visitor",
        "//mjlib/micro:async_stream",
        "//mjlib/micro:pool_ptr",
        "@boost",
    ],
    copts = [
        "-Wno-unknown-attributes",
    ],
)

cc_library(
    name = "micro_server",
    hdrs = ["micro_server.h"],
    srcs = ["micro_server.cc"],
    deps = [
        ":format",
        ":micro_datagram_server",
        ":stream",
        "//mjlib/base:assert",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:visitor",
        "//mjlib/micro:async_stream",
        "//mjlib/micro:persistent_config",
        "//mjlib/micro:pool_ptr",
        "@boost",
    ],
    copts = [
        "-Wno-unknown-attributes",
    ],
)

cc_library(
    name = "libmultiplex_tool",
    hdrs = ["multiplex_tool.h"],
    srcs = ["multiplex_tool.cc"],
    deps = [
        ":stream_asio_client_builder",
        "//mjlib/base:clipp",
        "//mjlib/base:error_code",
        "//mjlib/base:fail",
        "//mjlib/io:selector",
        "//mjlib/io:stream_copy",
        "//mjlib/io:stream_factory",
        "@boost",
        "@fmt",
    ],
)

cc_binary(
    name = "multiplex_tool",
    srcs = ["multiplex_tool_main.cc"],
    deps = [":libmultiplex_tool"],
)

cc_test(
    name = "test",
    srcs = [
        "test/stream_asio_client_test.cc",
        "test/fdcanusb_frame_stream_test.cc",
        "test/frame_test.cc",
        "test/rs485_frame_stream_test.cc",
        "test/register_test.cc",
        "test/test_main.cc",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            "test/micro_server_test.cc",
            "test/micro_stream_datagram_test.cc",
        ],
    }),
    deps = [
        ":stream_asio_client",
        ":frame",
        ":frame_stream",
        ":micro_stream_datagram",
        ":register",
        "//mjlib/io:stream_factory",
        "//mjlib/io:test_reader",
        "//mjlib/micro:stream_pipe",
        "//mjlib/micro:test_fixtures",
        "@boost//:test",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            ":micro_server",
        ],
    }),
    data = [
        ":multiplex_tool",
    ],
)

py_library(
    name = "py_stream_helpers",
    srcs = ["stream_helpers.py"],
)

py_library(
    name = "py_multiplex_protocol",
    srcs = ["multiplex_protocol.py"],
    deps = [":py_stream_helpers"],
)

py_binary(
    name = "mp_console",
    srcs = ["mp_console.py"],
    deps = [
        ":py_multiplex_protocol",
        ":aioserial",
    ],
)

py_binary(
    name = "conf_write",
    srcs = ["conf_write.py"],
    deps = [
        ":py_multiplex_protocol",
        ":aioserial",
    ],
)

py_test(
    name = "py_stream_helpers_test",
    srcs = ["test/py_stream_helpers_test.py"],
    deps = [":py_stream_helpers"],
)

py_test(
    name = "py_multiplex_protocol_test",
    srcs = ["test/py_multiplex_protocol_test.py"],
    deps = [":py_multiplex_protocol"],
)

py_library(
    name = "aioserial",
    srcs = ["aioserial.py"],
)

test_suite(
    name = "py_test",
    tests = [
        "py_stream_helpers_test",
        "py_multiplex_protocol_test",
    ],
)
