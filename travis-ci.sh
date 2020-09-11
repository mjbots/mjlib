#!/bin/sh

set -ev

if python3 -c "import snappy"; then
    # Nothing to do
    echo "Already have snappy"
else
    pip3 install python-snappy
fi

./tools/bazel test --copt -Werror //...
./tools/bazel build --copt -Werror --copt -Wdouble-promotion --cpu=stm32f4 -c opt //:target
