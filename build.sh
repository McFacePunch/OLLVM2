#!/bin/bash

# Check if an argument is provided for the build directory
if [ $# -eq 0 ]; then
    # No argument provided, use the default build directory
    mkdir -p build
    build_dir="build"
else
    # Argument provided, use it as the build directory
    build_dir="$1"
fi

# Determine the operating system
os_type=$(uname)

if [[ "$os_type" == "Linux" ]]; then
    # CMake command for Linux
    linux_cmake_command="cmake \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_ENABLE_NEW_PASS_MANAGER=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        -DLLVM_ENABLE_PROJECTS=\"clang;lld\" \
        -DLLVM_ENABLE_LLD=ON \
        -DLLVM_STATIC_LINK_CXX_STDLIB=ON \
        -S ./llvm-project/llvm \
        -B $build_dir \
        -G \"Ninja\""

    # Execute the Linux CMake command
    eval $linux_cmake_command

elif [[ "$os_type" == "Darwin" ]]; then
    # CMake command for macOS
    macos_cmake_command="cmake \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_ENABLE_NEW_PASS_MANAGER=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        -DLLVM_ENABLE_PROJECTS=\"clang;lld\" \
        -DLLVM_ENABLE_LLD=ON \
        -DLLVM_STATIC_LINK_CXX_STDLIB=ON \
        -DCMAKE_EXE_LINKER_FLAGS='-fuse-ld=lld' \
        -DCMAKE_SHARED_LINKER_FLAGS='-fuse-ld=lld' \
        -S ./llvm-project/llvm \
        -B $build_dir \
        -G Ninja"

    # Execute the macOS CMake command
    eval $macos_cmake_command

else
    echo "Unsupported operating system: $os_type"
    exit 1
fi

