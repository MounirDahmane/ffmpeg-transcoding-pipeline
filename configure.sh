#!/usr/bin/env bash

# Exit script if an error occurs
set -e
# Print trace of commands
set -x

mkdir -p build
mkdir -p frames

# Configure project using CMake
cmake -S.\
    -Bbuild \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "$@"