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
    name = "async_types",
    hdrs = ["async_types.h"],
    deps = [
        ":error_code",
        "//mjlib/base:inplace_function",
    ],
)

cc_library(
    name = "async_exclusive",
    hdrs = ["async_exclusive.h"],
    deps = [
        ":async_types",
        "//mjlib/base:assert",
        "//mjlib/base:inplace_function",
    ],
)

cc_library(
    name = "async_stream",
    hdrs = ["async_stream.h"],
    deps = [
        ":async_types",
        ":error_code",
        "//mjlib/base:noncopyable",
        "//mjlib/base:string_span",
    ],
)

cc_library(
    name = "atomic_event_queue",
    hdrs = ["atomic_event_queue.h"],
    deps = [
        "//mjlib/base:inplace_function",
    ],
)

cc_library(
    name = "callback_table",
    hdrs = ["callback_table.h"],
    srcs = ["callback_table.cc"],
    deps = [
        "//mjlib/base:inplace_function",
    ],
)

cc_library(
    name = "error_code",
    hdrs = ["error_code.h"],
    srcs = ["error_code.cc"],
)

cc_library(
    name = "error",
    hdrs = ["error.h"],
    srcs = ["error.cc"],
    deps = [
        ":error_code",
    ],
)

cc_library(
    name = "async_read",
    hdrs = ["async_read.h"],
    deps = [
        ":async_stream",
        ":error",
        "//mjlib/base:assert",
        "//mjlib/base:string_span",
    ],
)

cc_library(
    name = "pool_ptr",
    hdrs = ["pool_ptr.h"],
    srcs = ["pool_ptr.cc"],
    deps = ["//mjlib/base:assert"],
)

cc_library(
    name = "pool_map",
    hdrs = ["pool_map.h"],
    deps = [":pool_ptr"],
)

cc_library(
    name = "command_manager",
    hdrs = ["command_manager.h"],
    srcs = ["command_manager.cc"],
    deps = [
        ":async_exclusive",
        ":async_read",
        ":async_stream",
        ":async_types",
        ":pool_map",
        ":pool_ptr",
        "//mjlib/base:inplace_function",
        "//mjlib/base:string_span",
        "//mjlib/base:tokenizer",
    ],
)

cc_library(
    name = "event",
    hdrs = ["event.h"],
    deps = ["//mjlib/base:inplace_function"],
)

cc_library(
    name = "event_queue",
    hdrs = ["event_queue.h"],
    deps = ["event"],
)

cc_library(
    name = "persistent_config",
    hdrs = [
        "flash.h",
        "persistent_config.h",
    ],
    srcs = ["persistent_config.cc"],
    deps = [
        ":async_stream",
        ":command_manager",
        ":pool_map",
        ":pool_ptr",
        ":serializable_handler",
        "//mjlib/base:assert",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:crc",
        "//mjlib/base:crc_stream",
        "//mjlib/base:inplace_function",
        "//mjlib/base:noncopyable",
        "//mjlib/base:null_stream",
        "//mjlib/base:tokenizer",
    ],
)

cc_library(
    name = "required_success",
    hdrs = ["required_success.h"],
    deps = [":async_types"],
)

cc_library(
    name = "serializable_handler",
    hdrs = [
        "serializable_handler.h",
        "serializable_handler_detail.h",
    ],
    deps = [
        ":async_stream",
        ":async_types",
        "//mjlib/base:inplace_function",
        "//mjlib/base:stream",
        "//mjlib/base:string_span",
        "//mjlib/base:tokenizer",
        "//mjlib/base:visit_archive",
        "//mjlib/telemetry:binary_read_archive",
        "//mjlib/telemetry:binary_write_archive",
    ],
)

cc_library(
    name = "static_ptr",
    hdrs = ["static_ptr.h"],
)

cc_library(
    name = "static_vector",
    hdrs = ["static_vector.h"],
    deps = [
        "//mjlib/base:copy_const",
        "@boost",
    ],
)

cc_library(
    name = "stream_pipe",
    hdrs = ["stream_pipe.h"],
    srcs = ["stream_pipe.cc"],
    deps = [
        ":async_stream",
        ":event",
        "//mjlib/base:assert",
    ],
)

cc_library(
    name = "telemetry_manager",
    hdrs = ["telemetry_manager.h"],
    srcs = ["telemetry_manager.cc"],
    deps = [
        ":async_exclusive",
        ":async_stream",
        ":command_manager",
        ":pool_map",
        ":pool_ptr",
        ":serializable_handler",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:inplace_function",
        "//mjlib/base:stream",
        "//mjlib/base:tokenizer",
    ],
)


cc_library(
    name = "test_fixtures",
    hdrs = [
        "test/command_manager_fixture.h",
        "test/persistent_config_fixture.h",
        "test/reader.h",
        "test/str.h",
    ],
    deps = [
        ":event_queue",
        ":required_success",
    ],
)

filegroup(
    name = "target",
    srcs = [
        ":async_types",
        ":async_exclusive",
        ":async_stream",
        ":error_code",
        ":error",
        ":async_read",
        ":pool_ptr",
        ":pool_map",
        ":command_manager",
        ":event",
        ":event_queue",
        ":stream_pipe",
        ":serializable_handler",
        ":telemetry_manager",
        ":persistent_config",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/test_main.cc",
        "test/async_exclusive_test.cc",
        "test/atomic_event_queue_test.cc",
        "test/callback_table_test.cc",
        "test/error_code_test.cc",
        "test/pool_map_test.cc",
        "test/pool_ptr_test.cc",
        "test/static_ptr_test.cc",
        "test/static_vector_test.cc",
        "test/stream_pipe_test.cc",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            "test/async_stream_test.cc",
            "test/command_manager_test.cc",
            "test/persistent_config_test.cc",
            "test/serializable_handler_test.cc",
            "test/telemetry_manager_test.cc",
        ],
    }),
    deps = [
        ":async_exclusive",
        ":async_stream",
        ":atomic_event_queue",
        ":callback_table",
        ":error_code",
        ":event_queue",
        ":pool_map",
        ":pool_ptr",
        ":required_success",
        ":serializable_handler",
        ":static_ptr",
        ":static_vector",
        ":stream_pipe",
        ":test_fixtures",
        "//mjlib/base:buffer_stream",
        "@boost//:test",
        "@fmt",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [],
        "//conditions:default" : [
            ":command_manager",
            ":persistent_config",
            ":telemetry_manager",
        ],
    }),
)
