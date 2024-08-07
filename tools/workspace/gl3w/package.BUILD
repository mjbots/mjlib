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

py_binary(
    name = "gl3w_gen",
    srcs = ["gl3w_gen.py"],
)

genrule(
    name = "gen",
    srcs = ["include/GL/glcorearb.h", "include/GL/khrplatform.h"],
    outs = ["include/GL/gl3w.h", "src/gl3w.c"],
    tools = [":gl3w_gen"],
    cmd = (
        "OUTROOT=$$(dirname $(location include/GL/gl3w.h)); " +
        "mkdir -p $$OUTROOT && " +
        "cp $(location include/GL/glcorearb.h) $(location include/GL/khrplatform.h) $$OUTROOT && " +
        "$(location gl3w_gen) --root $$OUTROOT/../.."
    )
)

cc_library(
    name = "gl3w",
    hdrs = [
        "include/GL/gl3w.h",
        "include/GL/glcorearb.h",
        "include/GL/khrplatform.h",
    ],
    srcs = ["src/gl3w.c"],
    includes = ["include"],
)
