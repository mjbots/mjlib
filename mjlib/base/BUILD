# -*- python -*-

# Copyright 2018 Josh Pieper, jjp@pobox.com.
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
)

cc_library(
    name = "fast_stream",
    hdrs = ["fast_stream.h"],
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
    name = "system_error",
    hdrs = ["system_error.h"],
    srcs = ["system_error.cc"],
)

cc_test(
    name = "base_test",
    srcs = [
        "test/string_span_test.cc",
        "test/system_error_test.cc",
        "test/tokenizer_test.cc",
        "test/test_main.cc",
    ],
    deps = [
        ":string_span",
        ":system_error",
        ":tokenizer",
        "@boost//:test",
    ],
)
