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

load("//tools/workspace:github_archive.bzl", "github_archive")

def function2_repository():
    github_archive(
        name = "com_github_Naios_function2",
        repo = "Naios/function2",
        commit = "7cd95374b0f1c941892bfc40f0ebb6564d33fdb9",
        sha256 = "f2127da1c83d1c3dea8a416355a7cbb4e81fe55cf27ac9ef712d0554a3b745b6",
        build_file = Label("//tools/workspace/function2:package.BUILD"),
    )
