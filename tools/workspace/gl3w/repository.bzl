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

load("//tools/workspace:github_archive.bzl", "github_archive")

def _impl(repository_ctx):
    repo = "skaslev/gl3w"
    commit = "5f8d7fd191ba22ff2b60c1106d7135bb9a335533"
    sha256 = "e96a650a5fb9530b69a19d36ef931801762ce9cf5b51cb607ee116b908a380a6"

    repository_ctx.download_and_extract(
        url = ["https://github.com/{repo}/archive/{commit}.zip".format(
            repo=repo, commit=commit)],
        sha256 = sha256,
        stripPrefix = "{}-{}".format(repo.rsplit('/', 1)[-1], commit),
    )

    repository_ctx.file("WORKSPACE", "workspace(name = \"{name}\")\n".format(
        name=repository_ctx.name))

    repository_ctx.template("BUILD", repository_ctx.attr.build_file_template)
    repository_ctx.template("include/GL/glcorearb.h",
                            repository_ctx.attr.glcorearb)
    repository_ctx.template("include/GL/khrplatform.h",
                            repository_ctx.attr.khrplatform)

_gl3w_repository = repository_rule(
    implementation = _impl,
    attrs = {
        "build_file_template" : attr.label(allow_single_file=True),
        "glcorearb" : attr.label(allow_single_file=True),
        "khrplatform" : attr.label(allow_single_file=True),
    },
)

def gl3w_repository(name):
    _gl3w_repository(
        name = name,
        build_file_template = Label("//tools/workspace/gl3w:package.BUILD"),
        glcorearb = Label("//tools/workspace/gl3w:glcorearb.h"),
        khrplatform = Label("//tools/workspace/gl3w:khrplatform.h"),
    )
