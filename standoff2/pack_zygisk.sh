#!/usr/bin/env bash
# собирает libragemod.so и пакует Magisk/Zygisk-модуль: dist/ragemod-zygisk.zip
set -euo pipefail
cd "$(dirname "$0")"

./build.sh

VER=$(date +%Y%m%d)
OUT=dist/module
rm -rf dist
mkdir -p "$OUT/zygisk"

cp build/arm64/libragemod.so "$OUT/zygisk/arm64-v8a.so"

cat > "$OUT/module.prop" <<PROP
id=ragemod_so2
name=RageMod (Standoff 2)
version=$VER
versionCode=$VER
author=ruieod
description=Внутренний чит под com.axlebolt.standoff2 1.0.0 arm64. Грузится только в процесс игры.
PROP

( cd "$OUT" && zip -qr ../ragemod-zygisk.zip . )
ls -la dist/ragemod-zygisk.zip
