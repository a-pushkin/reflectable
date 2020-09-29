#!/bin/bash
git submodule update --init --force --recursive
mkdir -p build
cd build
CXXFLAGS="-fprofile-arcs -ftest-coverage"
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja ..
cp -f compile_commands.json ..
ninja
ctest -T Test