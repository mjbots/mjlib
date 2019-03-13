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
    deps = [":stream"],
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

cc_test(
    name = "test",
    srcs = [
        "test/pid_test.cc",
        "test/string_span_test.cc",
        "test/tokenizer_test.cc",
        "test/test_main.cc",
        "test/windowed_average_test.cc",
    ],
    deps = [
        ":pid",
        ":string_span",
        ":tokenizer",
        ":windowed_average",
        "@boost//:test",
    ],
)
