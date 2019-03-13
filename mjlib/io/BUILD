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
    ],
    deps = [
        ":async_stream",
        "//mjlib/base:system_error",
        "//mjlib/base:visitor",
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
        "test/test_main.cc",
    ],
    deps = [
        ":async_stream",
        ":stream_factory",
        "@boost//:test",
    ],
    data = [":stream_factory_manual_test"],
)
