# -*- python -*-

# Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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
    name = "args",
    hdrs = ["args.h"],
)

cc_library(
    name = "args_visitor",
    hdrs = ["args_visitor.h"],
    deps = [":visitor"],
)

cc_library(
    name = "assert",
    hdrs = ["assert.h"],
    srcs = ["assert.cc"],
    alwayslink = True,
)

cc_library(
    name = "bytes",
    hdrs = ["bytes.h"],
)

cc_library(
    name = "copy_const",
    hdrs = ["copy_const.h"],
)

cc_library(
    name = "fail",
    hdrs = ["fail.h"],
    srcs = ["fail.cc"],
    deps = [":error_code"],
)

cc_library(
    name = "inifile",
    hdrs = ["inifile.h"],
    srcs = ["inifile.cc"],
    deps = [
        ":system_error",
        "@boost",
        "@fmt",
    ],
)

cc_library(
    name = "inplace_function",
    hdrs = ["inplace_function.h"],
    deps = [":assert"],
)

cc_library(
    name = "stringify",
    hdrs = ["stringify.h"],
)

cc_library(
    name = "error_code",
    hdrs = ["error_code.h"],
    srcs = ["error_code.cc"],
    deps = [
        ":stringify",
        "@boost//:system",
    ],
)

cc_library(
    name = "error",
    hdrs = ["error.h"],
    srcs = ["error.cc"],
    deps = [
        "@boost//:system",
    ],
)

cc_library(
    name = "system_error",
    hdrs = ["system_error.h"],
    deps = [":error_code"],
)

cc_library(
    name = "stream",
    hdrs = ["stream.h"],
    deps = [":string_span"],
)

cc_library(
    name = "fast_stream",
    hdrs = ["fast_stream.h"],
    deps = [":stream"],
)

cc_library(
    name = "buffer_stream",
    hdrs = ["buffer_stream.h"],
    deps = [
        ":assert",
        ":stream",
    ],
)

cc_library(
    name = "crc_stream",
    hdrs = ["crc_stream.h"],
    deps = [
        ":stream",
    ],
)

cc_library(
    name = "file_stream",
    hdrs = ["file_stream.h"],
    deps = [":stream"],
)

cc_library(
    name = "null_stream",
    hdrs = ["null_stream.h"],
    deps = [":stream"],
)

cc_library(
    name = "string_span",
    hdrs = ["string_span.h"],
)

cc_library(
    name = "priority_tag",
    hdrs = ["priority_tag.h"],
)

cc_library(
    name = "visitor",
    hdrs = [
        "visitor.h",
        "detail/serialize.h",
    ],
    deps = [":priority_tag"],
)

cc_library(
    name = "visit_archive",
    hdrs = ["visit_archive.h"],
    deps = [":priority_tag", ":visitor"],
)

cc_library(
    name = "recording_stream",
    hdrs = ["recording_stream.h"],
    deps = [":stream", ":fast_stream"],
)

cc_library(
    name = "noncopyable",
    hdrs = ["noncopyable.h"],
)

cc_library(
    name = "tokenizer",
    hdrs = ["tokenizer.h"],
)

cc_library(
    name = "crc",
    hdrs = ["crc.h"],
    srcs = ["crc.cc"],
    deps = ["@boost"],
)

cc_library(
    name = "limit",
    hdrs = ["limit.h"],
)

cc_library(
    name = "pid",
    hdrs = ["pid.h"],
    deps = [
        ":limit",
        ":visitor",
    ],
)

cc_library(
    name = "windowed_average",
    hdrs = ["windowed_average.h"],
)

cc_library(
    name = "program_options_archive",
    hdrs = [
        "program_options_archive.h",
        "program_options_archive_detail.h",
    ],
    deps = [
        ":priority_tag",
        ":visitor",
        ":visit_archive",
        "@boost//:program_options",
    ],
)

cc_library(
    name = "clipp",
    hdrs = ["clipp.h"],
    deps = [
        ":inifile",
        "@com_github_muellan_clipp//:clipp"
    ],
)

cc_library(
    name = "clipp_archive",
    hdrs = ["clipp_archive.h"],
    deps = [
        ":args_visitor",
        ":system_error",
        ":visit_archive",
        "@com_github_muellan_clipp//:clipp",
    ],
)

cc_library(
    name = "collapse_whitespace",
    hdrs = ["collapse_whitespace.h"],
)

cc_library(
    name = "time_conversions",
    hdrs = [
        "time_conversions.h",
    ],
    srcs = [
        "time_conversions.cc",
    ],
    deps = [
        "@boost",
    ],
)

cc_library(
    name = "escape_json_string",
    hdrs = ["escape_json_string.h"],
    srcs = ["escape_json_string.cc"],
)

cc_library(
    name = "json5_write_archive",
    hdrs = ["json5_write_archive.h"],
    deps = [
        ":bytes",
        ":escape_json_string",
        ":priority_tag",
        ":visitor",
        ":visit_archive",
        "@boost//:date_time",
        "@fmt",
    ],
)

cc_library(
    name = "json5_read_archive",
    hdrs = ["json5_read_archive.h"],
    deps = [
        ":bytes",
        ":error",
        ":fail",
        ":system_error",
        ":visitor",
        ":visit_archive",
        "@fmt",
    ],
)

cc_library(
    name = "eigen",
    hdrs = ["eigen.h"],
    deps = [
        ":visitor",
        "@eigen",
    ],
)

cc_library(
    name = "aborting_posix_timer",
    hdrs = ["aborting_posix_timer.h"],
    srcs = ["aborting_posix_timer.cc"],
    deps = [
        ":fail",
        ":system_error",
    ],
)

cc_library(
    name = "system_fd",
    hdrs = ["system_fd.h"],
    srcs = ["system_fd.cc"],
)

cc_library(
    name = "system_file",
    hdrs = ["system_file.h"],
)

cc_library(
    name = "temporary_file",
    hdrs = ["temporary_file.h"],
    deps = ["@boost//:filesystem"],
)

cc_library(
    name = "thread_writer",
    hdrs = ["thread_writer.h"],
    deps = [
        ":system_fd",
        ":system_file",
        "@boost",
        "@boost//:filesystem",
    ],
)

cc_library(
    name = "all_types_struct",
    hdrs = ["test/all_types_struct.h"],
    deps = [
        ":bytes",
        ":time_conversions",
        ":visitor",
    ],
)

cc_binary(
    name = "aborting_posix_timer_manual_test",
    srcs = ["test/aborting_posix_timer_manual_test.cc"],
    deps = [
        ":aborting_posix_timer",
        "@boost//:program_options",
    ],
)

filegroup(
    name = "target",
    srcs = [
        ":bytes",
        ":inplace_function",
        ":stringify",
        ":stream",
        ":fast_stream",
        ":buffer_stream",
        ":crc_stream",
        ":string_span",
        ":priority_tag",
        ":visitor",
        ":visit_archive",
        ":recording_stream",
        ":noncopyable",
        ":tokenizer",
        ":crc",
        ":limit",
        ":windowed_average",
        ":time_conversions",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/buffer_stream_test.cc",
        "test/clipp_test.cc",
        "test/clipp_archive_test.cc",
        "test/crc_stream_test.cc",
        "test/eigen_test.cc",
        "test/error_code_test.cc",
        "test/external_serialize_test.cc",
        "test/inifile_test.cc",
        "test/inplace_function_test.cc",
        "test/json5_read_archive_test.cc",
        "test/json5_write_archive_test.cc",
        "test/limit_test.cc",
        "test/pid_test.cc",
        "test/program_options_archive_test.cc",
        "test/string_span_test.cc",
        "test/time_conversions_test.cc",
        "test/tokenizer_test.cc",
        "test/test_main.cc",
        "test/windowed_average_test.cc",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            "test/thread_writer_test.cc",
        ],
    }),

    deps = [
        ":all_types_struct",
        ":args",
        ":buffer_stream",
        ":clipp",
        ":clipp_archive",
        ":collapse_whitespace",
        ":crc_stream",
        ":eigen",
        ":error_code",
        ":fail",
        ":fast_stream",
        ":file_stream",
        ":inifile",
        ":inplace_function",
        ":json5_read_archive",
        ":json5_write_archive",
        ":limit",
        ":null_stream",
        ":pid",
        ":program_options_archive",
        ":string_span",
        ":system_error",
        ":temporary_file",
        ":time_conversions",
        ":tokenizer",
        ":windowed_average",
        "@boost//:test",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            ":system_fd",
            ":thread_writer",
        ]
    }),
    data = select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            ":aborting_posix_timer_manual_test",
        ],
    }),
)
