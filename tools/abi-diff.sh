#!/bin/sh

set -ex

ROOT_BUILD_DIR=${1:-.}
cd $ROOT_BUILD_DIR

# ldconfig is not in the path by default
hash abidiff /sbin/ldconfig

reflib="$(/sbin/ldconfig -p  | grep libmmlib | tr ' ' '\n' | grep /)"
testlib="./src/libmmlib.so"

abidiff $reflib $testlib
