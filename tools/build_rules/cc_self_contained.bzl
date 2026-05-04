# -*- python -*-
#
# Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

"""Drop-in replacement for `cc_library` that also enforces header
self-containment.

For every header listed in `hdrs`, the wrapper auto-generates a tiny
translation unit that does nothing except `#include` that one header,
and bundles those TUs into a sibling `cc_library` named
`<name>_self_contained_check`. The sibling depends only on the parent
library, so its include path resolves the parent's hdrs and the
parent's transitive deps' hdrs -- exactly what the parent needs to
compile -- and nothing more. If a header is not self-sufficient
(missing #include, missing inline keyword, ODR-violating non-inline
free function, ...) the sibling fails to build.

The generated TUs deliberately include nothing besides the header
under test, so order-of-inclusion bugs that would otherwise be masked
by a heavier prelude (e.g. <boost/test/...> transitively pulling in
<new>) still surface here.

Each BUILD file aggregates the per-library self-check siblings into a
single `:self_contained_check` target by calling
`self_contained_check_aggregate(name = "self_contained_check")` after
all `cc_library` calls; tests can then take a single
`:self_contained_check` dep to force the checks to build.
"""

# Suffix used on per-library self-check siblings. The aggregator
# below globs for this suffix to find them.
_CHECK_SUFFIX = "_self_contained_check"

def _self_contained_src_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + ".cc")
    ctx.actions.write(
        output = out,
        content = '#include "{}"\n'.format(ctx.attr.header),
    )
    return [DefaultInfo(files = depset([out]))]

_self_contained_src = rule(
    implementation = _self_contained_src_impl,
    attrs = {
        "header": attr.string(mandatory = True),
    },
)

def cc_library(name, hdrs = [], srcs = [], deps = [], testonly = False,
               visibility = None, **kwargs):
    """Wraps native.cc_library and emits a self-containment sibling.

    All arguments are forwarded to native.cc_library unchanged. If
    `hdrs` is non-empty, an additional `<name>_self_contained_check`
    cc_library is created whose sources are auto-generated TUs, one
    per header, each containing only `#include "<pkg>/<hdr>"`. Build
    failure of that sibling means a header is not self-contained.
    """
    native.cc_library(
        name = name,
        hdrs = hdrs,
        srcs = srcs,
        deps = deps,
        testonly = testonly,
        visibility = visibility,
        **kwargs
    )

    if not hdrs:
        return

    pkg = native.package_name()
    check_srcs = []
    for h in hdrs:
        full = (pkg + "/" + h) if pkg else h
        slug = full.replace("/", "_").replace(".", "_").replace("-", "_")
        src_target = name + _CHECK_SUFFIX + "_" + slug + "_src"
        _self_contained_src(
            name = src_target,
            header = full,
            visibility = ["//visibility:private"],
        )
        check_srcs.append(":" + src_target)

    native.cc_library(
        name = name + _CHECK_SUFFIX,
        srcs = check_srcs,
        # Depend on the parent library: its hdrs become visible to
        # our generated TUs, and its transitive deps' hdrs resolve
        # whatever the parent header pulls in.
        deps = [":" + name],
        testonly = True,
        visibility = ["//visibility:private"],
        tags = ["self_contained_check"],
    )

def self_contained_check_aggregate(name = "self_contained_check"):
    """Bundle every per-library self-containment sibling in this
    package into a single cc_library.

    Call this once at the bottom of a BUILD file after all
    cc_library declarations. A test target can then take a single
    dep on `:self_contained_check` to force every per-library check
    to build.
    """
    siblings = []
    for rule_name in native.existing_rules():
        if rule_name.endswith(_CHECK_SUFFIX):
            siblings.append(":" + rule_name)
    native.cc_library(
        name = name,
        deps = siblings,
        testonly = True,
    )
