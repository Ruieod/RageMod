#!/usr/bin/env bash
# Без рута: вшиваем libragemod.so в apk игры и добавляем его загрузку в точку входа.
# Нужны: apktool, java, apksigner (или uber-apk-signer), python3.
#   ./repack.sh /path/to/standoff2-base.apk
set -euo pipefail
cd "$(dirname "$0")"

APK="${1:-}"
[ -f "$APK" ] || { echo "usage: ./repack.sh /path/to/standoff2.apk"; exit 1; }
command -v apktool >/dev/null || { echo "нет apktool"; exit 1; }
command -v java >/dev/null || { echo "нет java"; exit 1; }

SO=build/arm64/libragemod.so
[ -f "$SO" ] || ./build.sh

WORK=work
DEC="$WORK/decoded"
rm -rf "$WORK"
mkdir -p "$WORK"

echo "== распаковка"
apktool d -f -o "$DEC" "$APK"

echo "== подкладываем либу"
mkdir -p "$DEC/lib/arm64-v8a"
cp "$SO" "$DEC/lib/arm64-v8a/libragemod.so"

echo "== патчим точку входа"
python3 tools/patch_smali.py "$DEC"

echo "== сборка"
apktool b "$DEC" -o "$WORK/unsigned.apk"

KS="$WORK/ragemod.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -v -keystore "$KS" -alias ragemod -keyalg RSA -keysize 2048 \
    -validity 10000 -storepass ragemod -keypass ragemod -dname "CN=ragemod"
fi

mkdir -p dist
if command -v apksigner >/dev/null; then
  apksigner sign --ks "$KS" --ks-pass pass:ragemod --key-pass pass:ragemod \
    --out dist/standoff2-ragemod.apk "$WORK/unsigned.apk"
else
  jarsigner -keystore "$KS" -storepass ragemod -keypass ragemod \
    -signedjar dist/standoff2-ragemod.apk "$WORK/unsigned.apk" ragemod
fi

ls -la dist/standoff2-ragemod.apk
cat <<'NOTE'

Дальше:
  adb install -r dist/standoff2-ragemod.apk
Если игра стоит сплитами (Play отдаёт base + config.*), ставь всё вместе и подписывай
тем же ключом:
  adb install-multiple dist/standoff2-ragemod.apk config.arm64_v8a.apk ...
Подпись изменилась — старые данные игры не подхватятся, и серверный чек подписи,
если он есть, может не пустить. Это цена варианта без рута.
NOTE
