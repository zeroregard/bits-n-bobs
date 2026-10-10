set -e
cd ~/src/SOES-NG
export CXXFLAGS="/permissive- /EHsc -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE /Zc:twoPhase- -D__cpp_lib_char8_t -D__cpp_consteval"
D=$HOME/src/soes-deps
[ -f build/build.ninja ] || cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$D/clangcl-xwin.cmake -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$D/prefix -Dabsl_DIR=$D/prefix/lib/cmake/absl -DProtobuf_DIR=$D/prefix/lib/cmake/protobuf \
  -Dutf8_range_DIR=$D/prefix/lib/cmake/utf8_range -DProtobuf_PROTOC_EXECUTABLE=$D/protoc/bin/protoc \
  -DProtobuf_INCLUDE_DIR=$D/prefix/include -DBUILD_TESTS=OFF -DENABLE_SKYRIM_VR=OFF -DCMAKE_PROJECT_INCLUDE=$D/protoc-target.cmake -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON -Dprotobuf_MODULE_COMPATIBLE=ON
cmake --build build -j12 -- "$@"
