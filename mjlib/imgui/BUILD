# -*- python -*-

# Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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
    name = "gl",
    hdrs = select({
        "@bazel_tools//src/conditions:windows" : [
         ],
        "//conditions:default" : [
            "gl.h",
         ],
    }),
)

cc_library(
    name = "imgui",
    hdrs = [
        "imgui_application.h",
    ],
    srcs = select({
        "@bazel_tools//src/conditions:windows" : [
            "windows_imgui_application.cc",
        ],
        "//conditions:default" : [
             "linux_imgui_application.cc",
        ],
    }),
    deps = [
        ":gl",
        "//mjlib/base:fail",
        "@eigen",
        "@fmt",
        "@imgui",
    ] + select({
        "@bazel_tools//src/conditions:windows" : [
        ],
        "//conditions:default" : [
            "@gl3w",
            "@glfw",
        ],
    }),
)
