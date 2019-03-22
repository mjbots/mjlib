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
        "//mjlib/base:system_error",
        "//mjlib/base:visitor",
        "@fmt",
    ],
)

cc_library(
    name = "debug_time",
    hdrs = [
        "virtual_deadline_timer.h",
        "deadline_timer.h",
    ],
    srcs = [
        "virtual_deadline_timer.cc",
    ],
    deps = [
        ":async_types",
    ],
    defines = [
        "BOOST_ASIO_ENABLE_OLD_SERVICES",
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
        ":stream_factory",
        "//mjlib/base:fail",
        "//mjlib/base:program_options_archive",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/async_stream_test.cc",
        "test/streambuf_read_stream_test.cc",
        "test/exclusive_command_test.cc",
        "test/test_main.cc",
        "test/virtual_deadline_timer_test.cc",
    ],
    deps = [
        ":async_stream",
        ":streambuf_read_stream",
        ":debug_time",
        ":exclusive_command",
        ":stream_factory",
        "@boost//:test",
    ],
    data = [":stream_factory_manual_test"],
)
