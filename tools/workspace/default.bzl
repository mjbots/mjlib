# -*- python -*-

# Copyright 2018-2020 Josh Pieper, jjp@pobox.com.
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

load("//tools/workspace/bazel_deps:repository.bzl", "bazel_deps_repository")
load("//tools/workspace/bazel:repository.bzl", "bazel_repository")
load("//tools/workspace/clipp:repository.bzl", "clipp_repository")
load("//tools/workspace/function2:repository.bzl", "function2_repository")
load("//tools/workspace/gl3w:repository.bzl", "gl3w_repository")
load("//tools/workspace/glfw:repository.bzl", "glfw_repository")
load("//tools/workspace/imgui:repository.bzl", "imgui_repository")
load("//tools/workspace/implot:repository.bzl", "implot_repository")
load("//tools/workspace/rules_mbed:repository.bzl", "rules_mbed_repository")
load("//tools/workspace/rules_nodejs:repository.bzl", "rules_nodejs_repository")

def add_default_repositories():
    if not native.existing_rule("com_github_mjbots_rules_mbed"):
        rules_mbed_repository()
    if not native.existing_rule("bazel"):
        bazel_repository()
    if not native.existing_rule("com_github_mjbots_bazel_deps"):
        bazel_deps_repository(name = "com_github_mjbots_bazel_deps")
    if not native.existing_rule("com_github_muellan_clipp"):
        clipp_repository(name = "com_github_muellan_clipp")
    if not native.existing_rule("build_bazel_rules_nodejs"):
        rules_nodejs_repository()
    if not native.existing_rule("function2"):
        function2_repository()
    if not native.existing_rule("gl3w"):
        gl3w_repository(name = "gl3w")
    if not native.existing_rule("glfw"):
        glfw_repository(name = "glfw")
    if not native.existing_rule("imgui"):
        imgui_repository(name = "imgui")
    if not native.existing_rule("implot"):
        implot_repository(name = "implot")
