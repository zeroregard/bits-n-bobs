#!/bin/bash
# Cross-build CourierLettersKit.dll on Linux (clang-cl + xwin), same toolchain as soes-ng-fix.
set -e
cd "$(dirname "$0")"
export CXXFLAGS="/permissive- /EHsc -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE /Zc:twoPhase- -D__cpp_lib_char8_t -D__cpp_consteval"
D=$HOME/src/soes-deps
[ -f build/build.ninja ] || cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$D/clangcl-xwin.cmake \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$D/prefix -DBUILD_TESTS=OFF -DENABLE_SKYRIM_VR=OFF \
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
cmake --build build -j12 -- "$@"
