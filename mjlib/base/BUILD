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
    name = "stream",
    hdrs = ["stream.h"],
)

cc_library(
    name = "fast_stream",
    hdrs = ["fast_stream.h"],
    deps = [":stream"],
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
