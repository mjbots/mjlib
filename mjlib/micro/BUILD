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
    name = "static_function",
    hdrs = ["static_function.h"],
    deps = ["//mjlib/base:assert"],
)

cc_library(
    name = "async_types",
    hdrs = ["async_types.h"],
    deps = [
        ":static_function",
        "//mjlib/base:system_error",
    ],
)

cc_library(
    name = "async_exclusive",
    hdrs = ["async_exclusive.h"],
    deps = [
        ":async_types",
        ":static_function",
        "//mjlib/base:assert",
    ],
)

cc_library(
    name = "async_stream",
    hdrs = ["async_stream.h"],
    deps = [
        ":async_types",
        ":static_function",
        "//mjlib/base:noncopyable",
        "//mjlib/base:string_span",
        "//mjlib/base:system_error",
    ],
)

cc_library(
    name = "error",
    hdrs = ["error.h"],
    srcs = ["error.cc"],
    deps = [
        "//mjlib/base:system_error",
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
        ":static_function",
        "//mjlib/base:string_span",
        "//mjlib/base:tokenizer",
    ],
)

cc_library(
    name = "event",
    hdrs = ["event.h"],
    deps = [":static_function"],
)

cc_library(
    name = "event_queue",
    hdrs = ["event_queue.h"],
    deps = ["event"],
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
    name = "serializable_handler",
    hdrs = [
        "serializable_handler.h",
        "serializable_handler_detail.h",
    ],
    deps = [
        ":async_stream",
        ":async_types",
        "//mjlib/base:stream",
        "//mjlib/base:string_span",
        "//mjlib/base:tokenizer",
        "//mjlib/base:visit_archive",
        "//mjlib/telemetry:telemetry_archive",
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
        ":static_function",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:stream",
        "//mjlib/base:tokenizer",
    ],
)

cc_library(
    name = "required_success",
    hdrs = ["required_success.h"],
    deps = [":async_types"],
)

cc_test(
    name = "micro_test",
    srcs = [
        "test/test_main.cc",
        "test/async_exclusive_test.cc",
        "test/async_stream_test.cc",
        "test/command_manager_test.cc",
        "test/pool_map_test.cc",
        "test/pool_ptr_test.cc",
        "test/reader.h",
        "test/serializable_handler_test.cc",
        "test/stream_pipe_test.cc",
        "test/telemetry_manager_test.cc",
    ],
    deps = [
        ":async_exclusive",
        ":async_stream",
        ":command_manager",
        ":event_queue",
        ":pool_map",
        ":pool_ptr",
        ":required_success",
        ":serializable_handler",
        ":stream_pipe",
        ":telemetry_manager",
        "//mjlib/base:buffer_stream",
        "@boost//:test",
        "@fmt",
    ],
)
