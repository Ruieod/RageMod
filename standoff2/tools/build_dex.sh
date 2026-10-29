#!/usr/bin/env bash
# Compiles the Java side (UI + Android glue) into build/out/classes.dex. Needs the Android SDK (platform + build-tools).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-/opt/android-sdk}}"
PLAT="$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)"
python3 "$ROOT/tools/gen_cfg.py"
rm -rf "$ROOT/build/classes" "$ROOT/build/out/dex"
mkdir -p "$ROOT/build/classes" "$ROOT/build/out/dex"
SRC=$(find "$ROOT/java" "$ROOT/build/gen/java" -name '*.java')
javac -source 8 -target 8 -Xlint:-options -Xlint:unchecked -encoding UTF-8 -bootclasspath "$PLAT/android.jar" -d "$ROOT/build/classes" $SRC
(cd "$ROOT/build/classes" && find . -name '*.class' | sort > "$ROOT/build/classes.list")
# build-tools' bundled d8 (8.2) crashes on some inner classes compiled by JDK 21; use a pinned newer R8 from Google Maven
R8="$ROOT/build/tools/r8-8.5.35.jar"
if [ ! -s "$R8" ]; then
  mkdir -p "$ROOT/build/tools"
  curl -fsSL -o "$R8" https://dl.google.com/dl/android/maven2/com/android/tools/r8/8.5.35/r8-8.5.35.jar
fi
java -cp "$R8" com.android.tools.r8.D8 --release --min-api 24 --lib "$PLAT/android.jar" --output "$ROOT/build/out/dex" $(find "$ROOT/build/classes" -name '*.class')
cp "$ROOT/build/out/dex/classes.dex" "$ROOT/build/out/classes.dex"
ls -la "$ROOT/build/out/classes.dex"
