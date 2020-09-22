#!/bin/bash
mkdir -p build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja ..
cp -f compile_commands.json ..
ninja
