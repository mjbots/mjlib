#!/bin/sh

set -ev

./tools/bazel test //...
./tools/bazel build --cpu=stm32f4 -c opt //:target
