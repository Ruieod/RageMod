#!/usr/bin/env bash
# Патчит уже готовый бэкап игры: SAI .apks / .xapk / отдельные apk / каталог с ними.
# NDK не нужен — нужна только собранная build/arm64/libragemod.so.
# apktool и uber-apk-signer скачиваются один раз в work/tools.
#
#   ./patch_apks.sh com.axlebolt.standoff2.apks
#   -> dist/standoff2-ragemod.apks  (ставить через SAI)
#   -> dist/apks/*.apk              (то же набором, для adb install-multiple)
set -euo pipefail
cd "$(dirname "$0")"

IN="${1:-}"
[ -e "$IN" ] || { echo "usage: ./patch_apks.sh <бэкап .apks|.xapk|.apk|каталог>"; exit 1; }
command -v java >/dev/null || { echo "нет java"; exit 1; }
command -v python3 >/dev/null || { echo "нет python3"; exit 1; }

SO=build/arm64/libragemod.so
[ -f "$SO" ] || { echo "нет $SO — положи готовую либу или собери: ANDROID_NDK=... ./build.sh"; exit 1; }

CACHE=work/tools
APKTOOL="$CACHE/apktool.jar"
SIGNER="$CACHE/uber-apk-signer.jar"
mkdir -p "$CACHE"
[ -f "$APKTOOL" ] || curl -sSL -o "$APKTOOL" \
  https://github.com/iBotPeaches/Apktool/releases/download/v2.9.3/apktool_2.9.3.jar
[ -f "$SIGNER" ] || curl -sSL -o "$SIGNER" \
  https://github.com/patrickfav/uber-apk-signer/releases/download/v1.3.0/uber-apk-signer-1.3.0.jar

WORK=work/apks
rm -rf "$WORK" dist
mkdir -p "$WORK/in" "$WORK/out" dist

echo "== разбираем вход"
if [ -d "$IN" ]; then
  cp "$IN"/*.apk "$WORK/in/"
elif [[ "$IN" == *.apk ]]; then
  cp "$IN" "$WORK/in/"
else
  unzip -qo "$IN" -d "$WORK/in"            # .apks / .xapk — это обычный zip
  find "$WORK/in" -name '*.apk' -exec mv -t "$WORK/in" {} + 2>/dev/null || true
fi
ls "$WORK/in"/*.apk >/dev/null 2>&1 || { echo "внутри нет ни одного apk"; exit 1; }

# base — тот, в котором есть classes.dex
BASE=""
for a in "$WORK/in"/*.apk; do
  if unzip -l "$a" | grep -q ' classes.dex$'; then BASE="$a"; break; fi
done
[ -n "$BASE" ] || { echo "не нашли apk с classes.dex"; exit 1; }
echo "   base: $(basename "$BASE")"

echo "== распаковка base (ресурсы не трогаем)"
java -jar "$APKTOOL" d -r -f -o "$WORK/decoded" "$BASE"

echo "== патчим точку входа"
python3 tools/patch_smali.py "$WORK/decoded"

echo "== подкладываем либу"
mkdir -p "$WORK/decoded/lib/arm64-v8a"
cp "$SO" "$WORK/decoded/lib/arm64-v8a/libragemod.so"
python3 - "$WORK/decoded/apktool.yml" <<'PY'
import sys
# .so должны лежать без сжатия: при extractNativeLibs=false иначе не загрузятся
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
if 'doNotCompress' not in s:
    s = s.rstrip('\n') + '\ndoNotCompress:\n- so\n'
elif '\n- so\n' not in s and '\n  - so\n' not in s:
    s = s.replace('doNotCompress:', 'doNotCompress:\n- so', 1)
open(p, 'w', encoding='utf-8').write(s)
PY

echo "== сборка base"
java -jar "$APKTOOL" b "$WORK/decoded" -o "$WORK/out/$(basename "$BASE")"

for a in "$WORK/in"/*.apk; do
  [ "$a" = "$BASE" ] && continue
  cp "$a" "$WORK/out/"
done

echo "== подпись (один ключ на все сплиты)"
java -jar "$SIGNER" --apks "$WORK/out" --allowResign --overwrite

mkdir -p dist/apks
cp "$WORK/out"/*.apk dist/apks/
( cd dist/apks && zip -q -0 ../standoff2-ragemod.apks ./*.apk )

ls -la dist dist/apks
cat <<'NOTE'

Готово. Ставить так:
  - снести оригинал (подпись другая, данные игры уйдут)
  - dist/standoff2-ragemod.apks скормить SAI
  или с ПК: adb install-multiple dist/apks/*.apk

Проверка: logcat -s RageMod -> "RageMod up (hooks 7/7)"
Меню: кнопка RAGE в левом верхнем углу.
NOTE
