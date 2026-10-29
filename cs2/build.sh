#!/usr/bin/env bash
# Сборка ragemod_cs2.dll (x64). Кросс-компиляция mingw-w64 с линукса или MSYS2 на винде.
set -euo pipefail
cd "$(dirname "$0")"

BUILD=build/win64
TOOLCHAIN=""
if command -v x86_64-w64-mingw32-g++ >/dev/null; then
  cat > "$BUILD.cmake" 2>/dev/null || mkdir -p "$(dirname "$BUILD.cmake")"
  cat > "$BUILD.cmake" <<'TC'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TC
  TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$PWD/$BUILD.cmake"
fi

cmake -B "$BUILD" -G Ninja $TOOLCHAIN -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"
ls -la "$BUILD"/ragemod_cs2.dll
