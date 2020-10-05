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
    name = "async_types",
    hdrs = ["async_types.h"],
    deps = [
        "//mjlib/base:error_code",
        "@boost",
        "@com_github_Naios_function2//:function2",
    ],
)

cc_library(
    name = "async_sequence",
    hdrs = ["async_sequence.h"],
    srcs = ["async_sequence.cc"],
    deps = [
        ":async_types",
        "//mjlib/base:assert",
    ],
)

cc_library(
    name = "async_stream",
    hdrs = ["async_stream.h"],
    deps = [
        ":async_types",
        "@boost",
    ],
)

cc_library(
    name = "stream_factory",
    hdrs = ["stream_factory.h",],
    srcs = [
        "stream_factory.cc",
        "stream_factory_stdio.h",
        "stream_factory_stdio.cc",
        "stream_factory_serial.h",
        "stream_factory_serial.cc",
        "stream_factory_tcp_client.h",
        "stream_factory_tcp_client.cc",
        "stream_factory_tcp_server.h",
        "stream_factory_tcp_server.cc",
        "stream_pipe_factory.h",
        "stream_pipe_factory.cc",
    ],
    deps = [
        ":async_stream",
        "//mjlib/base:args_visitor",
        "//mjlib/base:fail",
        "//mjlib/base:system_error",
        "@fmt",
    ],
)

cc_library(
    name = "debug_time",
    hdrs = [
        "deadline_timer.h",
        "debug_deadline_service.h",
        "timer_selector.h"
    ],
    srcs = [
        "timer_selector.cc",
    ],
    deps = [
        ":async_types",
        "//mjlib/base:fail",
        "//mjlib/base:system_error",
    ],
)

cc_library(
    name = "now",
    hdrs = ["now.h"],
    deps = [":debug_time"],
)

cc_library(
    name = "repeating_timer",
    hdrs = ["repeating_timer.h"],
    srcs = ["repeating_timer.cc"],
    deps = [
        ":debug_time",
        ":now",
    ],
)

cc_library(
    name = "realtime_executor",
    hdrs = ["realtime_executor.h"],
    deps = [
        "//mjlib/base:assert",
        "//mjlib/base:aborting_posix_timer",
    ],
)

cc_library(
    name = "exclusive_command",
    hdrs = ["exclusive_command.h"],
    deps = ["@boost",],
)

cc_library(
    name = "streambuf_read_stream",
    hdrs = ["streambuf_read_stream.h"],
    deps = [
        "//mjlib/base:stream",
        "@boost",
    ],
)

cc_library(
    name = "offset_buffer",
    hdrs = ["offset_buffer.h"],
    deps = [
        ":async_types",
        "@boost",
    ],
)

cc_library(
    name = "selector",
    hdrs = ["selector.h"],
    deps = [
        ":async_types",
        "//mjlib/base:clipp_archive",
        "//mjlib/base:system_error",
        "@boost",
    ],
)

cc_library(
    name = "stream_copy",
    hdrs = ["stream_copy.h"],
    deps = [
        ":async_stream",
        ":async_types",
        "//mjlib/base:error_code",
        "//mjlib/base:fail",
        "@boost",
    ],
)

cc_library(
    name = "test_reader",
    hdrs = ["test/reader.h"],
    deps = [
        ":async_stream",
        "//mjlib/base:fail",
        "@boost",
    ],
)

cc_binary(
    name = "stream_factory_manual_test",
    srcs = ["test/stream_factory_manual_test.cc",],
    deps = [
        ":stream_copy",
        ":stream_factory",
        "//mjlib/base:fail",
        "//mjlib/base:clipp",
        "//mjlib/base:clipp_archive",
    ],
)

cc_binary(
    name = "realtime_executor_manual_test",
    srcs = ["test/realtime_executor_manual_test.cc"],
    deps = [
        ":realtime_executor",
        ":repeating_timer",
        "@boost//:program_options",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/async_sequence_test.cc",
        "test/async_stream_test.cc",
        "test/async_types_test.cc",
        "test/exclusive_command_test.cc",
        "test/offset_buffer_test.cc",
        "test/repeating_timer_test.cc",
        "test/selector_test.cc",
        "test/streambuf_read_stream_test.cc",
        "test/test_main.cc",
        "test/virtual_deadline_timer_test.cc",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            "test/realtime_executor_test.cc",
        ],
    }),
    deps = [
        ":async_sequence",
        ":async_stream",
        ":debug_time",
        ":exclusive_command",
        ":offset_buffer",
        ":repeating_timer",
        ":selector",
        ":streambuf_read_stream",
        ":stream_factory",
        "//mjlib/base:clipp",
        "//mjlib/base:collapse_whitespace",
        "@boost//:test",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": [
            ":realtime_executor",
        ],
    }),
    data = [
        ":stream_factory_manual_test",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default" : [
            ":realtime_executor_manual_test",
        ],
    }),
)
