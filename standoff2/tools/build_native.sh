#!/usr/bin/env bash
# Builds libragemod.so for arm64-v8a. Needs ANDROID_NDK_HOME (or ANDROID_HOME/ndk/<ver>).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [ -z "$NDK" ]; then NDK="$(ls -d "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-/opt/android-sdk}}"/ndk/* | sort -V | tail -1)"; fi
python3 "$ROOT/tools/gen_cfg.py"
GEN=(); command -v ninja >/dev/null && GEN=(-G Ninja)
cmake -S "$ROOT/native" -B "$ROOT/build/native" "${GEN[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/native" -j"$(nproc)"
mkdir -p "$ROOT/build/out/lib/arm64-v8a"
cp "$ROOT/build/native/libragemod.so" "$ROOT/build/out/lib/arm64-v8a/libragemod.so"
"$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip --strip-unneeded "$ROOT/build/out/lib/arm64-v8a/libragemod.so"
ls -la "$ROOT/build/out/lib/arm64-v8a/libragemod.so"
