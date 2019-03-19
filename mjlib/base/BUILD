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
    name = "assert",
    hdrs = ["assert.h"],
    srcs = ["assert.cc"],
    alwayslink = True,
)

cc_library(
    name = "fail",
    hdrs = ["fail.h"],
    srcs = ["fail.cc"],
    deps = [":error_code"],
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
    name = "string_span",
    hdrs = ["string_span.h"],
)

cc_library(
    name = "visitor",
    hdrs = ["visitor.h"],
)

cc_library(
    name = "visit_archive",
    hdrs = ["visit_archive.h"],
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
        ":visitor",
        ":visit_archive",
        "@boost//:program_options",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/buffer_stream_test.cc",
        "test/error_code_test.cc",
        "test/pid_test.cc",
        "test/program_options_archive_test.cc",
        "test/string_span_test.cc",
        "test/tokenizer_test.cc",
        "test/test_main.cc",
        "test/windowed_average_test.cc",
    ],
    deps = [
        ":buffer_stream",
        ":error_code",
        ":fail",
        ":pid",
        ":program_options_archive",
        ":string_span",
        ":system_error",
        ":tokenizer",
        ":windowed_average",
        "@boost//:test",
    ],
)
