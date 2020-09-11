#!/bin/sh

set -ev

python3 -c "import snappy"
if [ $? -ne 0 ]; then
    pip3 install snappy
fi

./tools/bazel test --copt -Werror //...
./tools/bazel build --copt -Werror --copt -Wdouble-promotion --cpu=stm32f4 -c opt //:target
