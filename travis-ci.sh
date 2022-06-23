#!/bin/sh

set -ev

if python3 -c "import snappy"; then
    # Nothing to do
    echo "Already have snappy"
else
    pip3 install python-snappy
fi

./tools/bazel test --config=host //...
./tools/bazel build --config=target -c opt //:target
