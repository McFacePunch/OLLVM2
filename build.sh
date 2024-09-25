#!/bin/bash

# Check if an argument is provided
if [ $# -eq 0 ]; then
    # No argument provided, use the default build directory
    mkdir build
    build_dir="build"
else
    # Argument provided, use it as the build directory
    build_dir="$1"
fi

# Construct the cmake command with the chosen build directory
cmake_command="cmake \
    -DLLVM_BUILD_TOOLS=ON \
    -DLLVM_ENABLE_NEW_PASS_MANAGER=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_ENABLE_PROJECTS=\"clang;lld\" \
    -DLLVM_ENABLE_LLD=ON \
    -DLLVM_STATIC_LINK_CXX_STDLIB=ON \
    -S ./llvm-project/llvm \
    -B $build_dir \
    -G \"Ninja\""

# Execute the cmake command
eval $cmake_command
