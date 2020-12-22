# -*- python -*-

# Copyright 2020 Josh Pieper, jjp@pobox.com.
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

load("//tools/workspace:github_archive.bzl", "github_archive")

def glfw_repository(name):
    github_archive(
        name = name,
        repo = "glfw/glfw",
        commit = "0b9e48fa3df9c184ff1abfb2452fd1a4b696ecd8",
        sha256 = "454148695e929bb6577b1c89eef193d52646e09c551d9d66993ddd0dee29d39d",
        build_file = Label("//tools/workspace/glfw:package.BUILD"),
    )
