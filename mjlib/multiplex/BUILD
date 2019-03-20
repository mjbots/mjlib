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
    name = "format",
    hdrs = ["format.h"],
)

cc_library(
    name = "stream",
    hdrs = ["stream.h"],
    deps = [
        ":format",
        "//mjlib/base:assert",
        "//mjlib/base:stream",
    ],
)

cc_library(
    name = "register",
    hdrs = ["register.h"],
    srcs = ["register.cc"],
    deps = [
        ":format",
        ":stream",
        "//mjlib/base:assert",
        "//mjlib/base:fail",
        "//mjlib/base:fast_stream",
        "//mjlib/base:stream",
    ],
)

cc_library(
    name = "asio_client",
    hdrs = ["asio_client.h"],
    srcs = ["asio_client.cc"],
    deps = [
        ":register",
        "//mjlib/base:error_code",
        "//mjlib/io:async_stream",
        "//mjlib/io:exclusive_command",
    ],
)

cc_library(
    name = "micro_server",
    hdrs = ["micro_server.h"],
    srcs = ["micro_server.cc"],
    deps = [
        ":format",
        ":stream",
        "//mjlib/base:assert",
        "//mjlib/base:buffer_stream",
        "//mjlib/base:visitor",
        "//mjlib/micro:async_stream",
        "//mjlib/micro:persistent_config",
        "//mjlib/micro:pool_ptr",
        "@boost",
    ],
)

cc_test(
    name = "test",
    srcs = [
        "test/micro_server_test.cc",
        "test/register_test.cc",
        "test/test_main.cc",
    ],
    deps = [
        ":asio_client",
        ":micro_server",
        ":register",
        "//mjlib/micro:stream_pipe",
        "//mjlib/micro:test_fixtures",
        "@boost//:test",
    ],
)

py_library(
    name = "py_stream_helpers",
    srcs = ["stream_helpers.py"],
)

py_library(
    name = "py_multiplex_protocol",
    srcs = ["multiplex_protocol.py"],
    deps = [":py_stream_helpers"],
)

py_binary(
    name = "mp_console",
    srcs = ["mp_console.py"],
    deps = [
        ":py_multiplex_protocol",
        ":aioserial",
    ],
)

py_binary(
    name = "conf_write",
    srcs = ["conf_write.py"],
    deps = [
        ":py_multiplex_protocol",
        ":aioserial",
    ],
)

py_test(
    name = "py_stream_helpers_test",
    srcs = ["test/py_stream_helpers_test.py"],
    deps = [":py_stream_helpers"],
)

py_test(
    name = "py_multiplex_protocol_test",
    srcs = ["test/py_multiplex_protocol_test.py"],
    deps = [":py_multiplex_protocol"],
)

py_library(
    name = "aioserial",
    srcs = ["aioserial.py"],
)

test_suite(
    name = "py_test",
    tests = [
        "py_stream_helpers_test",
        "py_multiplex_protocol_test",
    ],
)
