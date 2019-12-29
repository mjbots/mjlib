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

load("//tools/workspace:github_archive.bzl", "github_archive")

def clipp_repository(name):
    github_archive(
        name = name,
        repo = "muellan/clipp",
        commit = "2c32b2f1f7cc530b1ec1f62c92f698643bb368db",
        sha256 = "9ee5f3b5ab23c6c05dfd6905317d2aab31eaa507a45d50cd8c318ecd32682861",
        build_file = Label("//tools/workspace/clipp:package.BUILD"),
    )
