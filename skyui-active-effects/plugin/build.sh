#!/bin/sh
# Cross-compile the AEC SKSE plugin (Windows x64 DLL) on Linux with clang-cl + xwin.
set -e
. "$(dirname "$0")/env.sh"
cd "$(dirname "$0")"; mkdir -p obj
INC="/I skse /I skse/skse64 /I common /FI common/IPrefix.h"
DEFS="/D _CRT_SECURE_NO_WARNINGS /D RUNTIME /D RUNTIME_VERSION=0x01064920 /D NOMINMAX"
OBJS=""
for f in src/main.cpp $EXTRA; do
  o=obj/$(echo "$f" | tr '/' '_' | sed 's/\.cpp$/.obj/')
  clang-cl $CXXFLAGS $DEFS $INC /c "$f" /Fo"$o"
  OBJS="$OBJS $o"
done
lld-link /nologo /dll /out:ActiveEffectCategories.dll $OBJS $LDFLAGS kernel32.lib user32.lib shell32.lib ole32.lib advapi32.lib
echo BUILD-OK
