#!/usr/bin/env bash
# сборка libragemod.so (arm64-v8a). ANDROID_NDK — путь к NDK r25+.
set -euo pipefail
cd "$(dirname "$0")"

NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/27.2.12479018}}"
[ -d "$NDK" ] || { echo "нет NDK: $NDK (задай ANDROID_NDK)"; exit 1; }

BUILD=build/arm64
cmake -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"

"$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip --strip-unneeded "$BUILD/libragemod.so"
ls -la "$BUILD/libragemod.so"
