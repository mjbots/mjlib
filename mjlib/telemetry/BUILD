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
    name = "format",
    hdrs = [
        "format.h",
        "types.h",
    ],
    deps = [
        "//mjlib/base:assert",
        "//mjlib/base:time_conversions",
        "//mjlib/base:system_error",
        "@boost",
    ],
)

cc_library(
    name = "binary_write_archive",
    hdrs = [
        "binary_write_archive.h",
    ],
    deps = [
        ":format",
        "//mjlib/base:priority_tag",
        "//mjlib/base:visitor",
        "//mjlib/base:visit_archive",
    ],
)

cc_library(
    name = "telemetry_format",
    hdrs = ["telemetry_format.h"],
    deps = [
        "//mjlib/base:assert",
        "//mjlib/base:time_conversions",
    ],
)

cc_library(
    name = "telemetry_archive",
    hdrs = [
        "telemetry_archive.h",
        "telemetry_archive_detail.h",
    ],
    deps = [
        ":telemetry_format",
        "//mjlib/base:fast_stream",
        "//mjlib/base:stream",
        "//mjlib/base:visit_archive",
        "//mjlib/base:visitor",
    ],
)

cc_library(
    name = "telemetry_util",
    hdrs = ["telemetry_util.h"],
    deps = [
        "//mjlib/base:assert",
        "//mjlib/base:recording_stream",
        "//mjlib/base:stringify",
        "@fmt",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/binary_write_archive_test.cc",
        "test/format_test.cc",
        "test/telemetry_archive_test.cc",
        "test/test_main.cc",

        "test/all_types_struct.h",
        "test/test_util.h",
    ],
    deps = [
        ":format",
        ":binary_write_archive",
        ":telemetry_archive",
        ":telemetry_util",
        "@boost//:test",
        "@fmt",
    ],
)
