#!/usr/bin/env bash
# Без рута: вшиваем libragemod.so в apk игры и добавляем его загрузку в точку входа.
# Нужны: apktool, java, apksigner (или jarsigner), python3, adb (для --device).
#
#   ./repack.sh --device              вытащить сплиты с устройства и пропатчить
#                                     (с ПК — через adb, из Termux на самом телефоне — через pm)
#   ./repack.sh base.apk [split...]   то же по локальным файлам
#
# Все сплиты подписываются одним ключом — иначе install-multiple их не примет.
set -euo pipefail
cd "$(dirname "$0")"

PKG=com.axlebolt.standoff2
WORK=work
DEC="$WORK/decoded"
KS="$WORK/ragemod.keystore"
PASS=ragemod

command -v apktool >/dev/null || { echo "нет apktool"; exit 1; }
command -v java >/dev/null || { echo "нет java"; exit 1; }

SO=build/arm64/libragemod.so
if [ ! -f "$SO" ]; then
  if [ -n "${ANDROID_NDK:-}${ANDROID_NDK_HOME:-}" ]; then
    ./build.sh
  else
    echo "нет $SO."
    echo "Положи готовую либу в $SO (артефакт CI или из чата) либо собери: ANDROID_NDK=... ./build.sh"
    exit 1
  fi
fi

rm -rf "$WORK" dist
mkdir -p "$WORK/orig" dist

APKS=()
VIA_ADB=0
if [ "${1:-}" = "--device" ]; then
  if command -v adb >/dev/null; then
    VIA_ADB=1
    echo "== тянем установленные apk через adb (рут не нужен)"
    mapfile -t paths < <(adb shell pm path "$PKG" | tr -d '\r' | sed 's/^package://')
  elif command -v pm >/dev/null; then
    echo "== тянем установленные apk через pm (Termux на самом телефоне, рут не нужен)"
    mapfile -t paths < <(pm path "$PKG" | tr -d '\r' | sed 's/^package://')
  else
    echo "нет ни adb, ни pm"; exit 1
  fi
  [ "${#paths[@]}" -gt 0 ] || { echo "игра не установлена"; exit 1; }
  for p in "${paths[@]}"; do
    name=$(basename "$p")
    if [ "$VIA_ADB" = 1 ]; then
      adb pull "$p" "$WORK/orig/$name" >/dev/null
    else
      cp "$p" "$WORK/orig/$name"
    fi
    echo "   $name"
    APKS+=("$WORK/orig/$name")
  done
else
  [ -f "${1:-}" ] || { echo "usage: ./repack.sh --device | ./repack.sh base.apk [split...]"; exit 1; }
  for f in "$@"; do
    cp "$f" "$WORK/orig/$(basename "$f")"
    APKS+=("$WORK/orig/$(basename "$f")")
  done
fi

# base — тот, где есть AndroidManifest с application; обычно так и называется
BASE=""
for a in "${APKS[@]}"; do
  case "$(basename "$a")" in
    base.apk|*base*.apk) BASE="$a"; break;;
  esac
done
[ -n "$BASE" ] || BASE="${APKS[0]}"
echo "== base: $(basename "$BASE")"

echo "== распаковка"
apktool d -f -o "$DEC" "$BASE"

echo "== подкладываем либу"
mkdir -p "$DEC/lib/arm64-v8a"
cp "$SO" "$DEC/lib/arm64-v8a/libragemod.so"

echo "== патчим точку входа"
python3 tools/patch_smali.py "$DEC"

echo "== сборка base"
apktool b "$DEC" -o "$WORK/base-unsigned.apk"

if [ ! -f "$KS" ]; then
  keytool -genkeypair -v -keystore "$KS" -alias ragemod -keyalg RSA -keysize 2048 \
    -validity 10000 -storepass "$PASS" -keypass "$PASS" -dname "CN=ragemod" >/dev/null
fi

sign() { # $1 вход, $2 выход
  if command -v apksigner >/dev/null; then
    apksigner sign --ks "$KS" --ks-pass "pass:$PASS" --key-pass "pass:$PASS" --out "$2" "$1"
  else
    cp "$1" "$2"
    jarsigner -keystore "$KS" -storepass "$PASS" -keypass "$PASS" "$2" ragemod >/dev/null
  fi
}

echo "== подписываем"
sign "$WORK/base-unsigned.apk" "dist/base.apk"
OUT=(dist/base.apk)
for a in "${APKS[@]}"; do
  [ "$a" = "$BASE" ] && continue
  name=$(basename "$a")
  sign "$a" "dist/$name"
  OUT+=("dist/$name")
done

ls -la dist/
echo
echo "Подпись другая, поэтому старую установку надо снести — данные игры уйдут."
if command -v adb >/dev/null; then
cat <<NOTE
Ставить так:
  adb uninstall $PKG
  adb install-multiple ${OUT[*]}
NOTE
else
cat <<NOTE
Ставить сплиты без adb: снести игру вручную, потом скормить всё из dist/
сплит-установщику (SAI / Split APKs Installer) — обычным тапом по apk сплиты не ставятся.
NOTE
fi
cat <<'NOTE'
Проверка: logcat -s RageMod  -> "RageMod up (hooks 7/7)"
Меню: кнопка RAGE в левом верхнем углу.
NOTE
