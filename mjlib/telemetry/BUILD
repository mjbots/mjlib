# -*- python -*-

# Copyright 2018-2019 Josh Pieper, jjp@pobox.com.
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
    name = "error",
    hdrs = ["error.h"],
    srcs = ["error.cc"],
    deps = [
        "//mjlib/base:error_code",
        "@boost",
    ],
)

cc_library(
    name = "format",
    hdrs = ["format.h"],
    deps = [
        "//mjlib/base:assert",
        "//mjlib/base:bytes",
        "//mjlib/base:time_conversions",
        "//mjlib/base:stream",
        "//mjlib/base:system_error",
        "@boost",
    ],
)

cc_library(
    name = "binary_write_archive",
    hdrs = ["binary_write_archive.h"],
    deps = [
        ":format",
        "//mjlib/base:fast_stream",
        "//mjlib/base:priority_tag",
        "//mjlib/base:stream",
        "//mjlib/base:visitor",
        "//mjlib/base:visit_archive",
    ],
)

cc_library(
    name = "binary_read_archive",
    hdrs = ["binary_read_archive.h"],
    deps = [
        ":format",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:priority_tag",
        "//mjlib/base:stream",
        "//mjlib/base:visitor",
        "//mjlib/base:visit_archive",
    ],
)

cc_library(
    name = "binary_schema_parser",
    hdrs = ["binary_schema_parser.h"],
    srcs = ["binary_schema_parser.cc"],
    deps = [
        ":error",
        ":format",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:fail",
        "//mjlib/base:recording_stream",
        "//mjlib/base:stream",
        "//mjlib/base:system_error",
        "@boost",
        "@fmt",
    ],
)

cc_library(
    name = "emit_json",
    hdrs = ["emit_json.h"],
    srcs = ["emit_json.cc"],
    deps = [
        ":binary_schema_parser",
        ":format",
        "//mjlib/base:escape_json_string",
        "//mjlib/base:fail",
        "//mjlib/base:stream",
        "//mjlib/base:time_conversions",
    ],
)

cc_library(
    name = "mapped_binary_reader",
    hdrs = [
        "container_types.h",
        "mapped_binary_reader.h",
    ],
    deps = [
        ":binary_read_archive",
        ":binary_schema_parser",
        ":binary_write_archive",
    ],
)

cc_library(
    name = "file_writer",
    hdrs = ["file_writer.h"],
    srcs = ["file_writer.cc"],
    deps = [
        ":format",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:fail",
        "//mjlib/base:fast_stream",
        "//mjlib/base:system_error",
        "//mjlib/base:thread_writer",
        "@boost",
        "@fmt",
        "@snappy",
    ],
)

cc_library(
    name = "file_reader",
    hdrs = ["file_reader.h"],
    srcs = ["file_reader.cc"],
    deps = [
        ":binary_schema_parser",
        ":format",
        "//mjlib/base:crc_stream",
        "//mjlib/base:file_stream",
        "@snappy",
    ],
)

cc_binary(
    name = "file_json_dump",
    srcs = ["file_json_dump.cc"],
    deps = [
        ":emit_json",
        ":file_reader",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:clipp",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/binary_read_archive_test.cc",
        "test/binary_schema_parser_test.cc",
        "test/binary_write_archive_test.cc",
        "test/emit_json_test.cc",
        "test/format_test.cc",
        "test/mapped_binary_reader_test.cc",
        "test/test_main.cc",

        "test/test_util.h",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default" : [
            "test/file_reader_test.cc",
            "test/file_writer_test.cc",
        ],
    }),
    deps = [
        ":binary_read_archive",
        ":binary_schema_parser",
        ":binary_write_archive",
        ":emit_json",
        ":error",
        ":format",
        ":file_reader",
        ":mapped_binary_reader",
        "//mjlib/base:all_types_struct",
        "//mjlib/base:temporary_file",
        "@boost//:test",
        "@boost//:date_time",
        "@fmt",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            ":file_writer",
        ],
    }),
    data = [
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            "//mjlib/telemetry/js",
            # Just so it is built.
            ":file_json_dump",
        ],
    }),
)

py_library(
    name = "py_reader",
    srcs = ["reader.py"],
)

py_library(
    name = "py_file_reader",
    srcs = ["file_reader.py"],
    deps = [":py_reader"],
)

py_test(
    name = "py_reader_test",
    srcs = ["test/py_reader_test.py"],
    deps = [":py_reader"],
)

py_test(
    name = "py_file_reader_test",
    srcs = ["test/py_file_reader_test.py"],
    deps = [":py_file_reader"],
)

test_suite(
    name = "py_test",
    tests = [
        "py_reader_test",
        "py_file_reader_test",
    ],
)
