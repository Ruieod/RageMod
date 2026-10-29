#!/usr/bin/env bash
# Вшивает чит в apk без правки dex: оригинальный lib/arm64-v8a/libmain.so переименовывается
# в libmain_orig.so, на его место кладётся наша либа (у неё есть JNI_OnLoad, который
# пробрасывает вызов в оригинал). Unity сам зовёт System.loadLibrary("main") на старте.
#
#   ./tools/pack_apk.sh <game.apk|game.apks> [out.apk]
set -euo pipefail
cd "$(dirname "$0")/.."

IN="${1:-}"
OUT="${2:-dist/standoff2-ragemod.apk}"
[ -f "$IN" ] || { echo "usage: ./tools/pack_apk.sh <game.apk|game.apks> [out.apk]"; exit 1; }
command -v java >/dev/null || { echo "нет java"; exit 1; }
command -v zip >/dev/null || { echo "нет zip"; exit 1; }

SO=build/arm64/libragemod.so
[ -f "$SO" ] || { echo "нет $SO — положи готовую либу или собери: ANDROID_NDK=... ./build.sh"; exit 1; }

CACHE=work/tools
SIGNER="$CACHE/uber-apk-signer.jar"
mkdir -p "$CACHE"
[ -f "$SIGNER" ] || curl -sSL -o "$SIGNER" \
  https://github.com/patrickfav/uber-apk-signer/releases/download/v1.3.0/uber-apk-signer-1.3.0.jar

WORK=work/pack
rm -rf "$WORK"
mkdir -p "$WORK/stage/lib/arm64-v8a" "$(dirname "$OUT")"

SRC="$IN"
if [[ "$IN" != *.apk ]]; then
  echo "== распаковываем бэкап"
  mkdir -p "$WORK/in"
  unzip -qo "$IN" -d "$WORK/in"
  SRC=""
  for f in "$WORK"/in/*.apk; do
    # grep -q рвёт пайп и ловит pipefail, поэтому считаем совпадения
    if [ "$(unzip -l "$f" | grep -c 'classes\.dex')" -gt 0 ]; then SRC="$f"; break; fi
  done
  [ -n "$SRC" ] || { echo "в бэкапе нет apk с classes.dex"; exit 1; }
  COUNT=$(ls "$WORK"/in/*.apk | wc -l)
  [ "$COUNT" = 1 ] || echo "   внимание: в бэкапе $COUNT apk (сплиты) — остальные надо подписать тем же ключом"
fi
echo "== база: $(basename "$SRC")"

[ "$(unzip -l "$SRC" | grep -c 'lib/arm64-v8a/libmain\.so')" -gt 0 ] || { echo "в apk нет lib/arm64-v8a/libmain.so"; exit 1; }

cp "$SRC" "$WORK/out.apk"

echo "== вынимаем оригинальный libmain.so"
unzip -qo "$WORK/out.apk" lib/arm64-v8a/libmain.so -d "$WORK/orig"
mv "$WORK/orig/lib/arm64-v8a/libmain.so" "$WORK/stage/lib/arm64-v8a/libmain_orig.so"

echo "== наша либа под именем libmain.so"
python3 - "$SO" "$WORK/stage/lib/arm64-v8a/libmain.so" <<'PY'
import sys
# soname меняем на libmain.so: строка короче, так что правим на месте, смещения не едут
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
old, new = b'libragemod.so\x00', b'libmain.so\x00\x00\x00\x00'
i = d.find(old)
if i < 0:
    raise SystemExit('не нашли soname libragemod.so')
d[i:i + len(old)] = new
open(dst, 'wb').write(bytes(d))
print('   soname -> libmain.so')
PY

echo "== пересобираем zip"
zip -q -d "$WORK/out.apk" 'META-INF/*.SF' 'META-INF/*.RSA' 'META-INF/*.DSA' 'META-INF/MANIFEST.MF' >/dev/null 2>&1 || true
zip -q -d "$WORK/out.apk" 'lib/arm64-v8a/libmain.so' >/dev/null
( cd "$WORK/stage" && zip -q -g "../out.apk" lib/arm64-v8a/libmain_orig.so lib/arm64-v8a/libmain.so )

echo "== подпись"
java -jar "$SIGNER" -a "$WORK/out.apk" --allowResign --overwrite >/dev/null
cp "$WORK/out.apk" "$OUT"

ls -la "$OUT"
cat <<NOTE

Готово: $OUT
Ставить: снести оригинал (подпись другая, данные игры уйдут) и поставить этот apk
обычным тапом или через SAI.
Проверка: logcat -s RageMod -> "RageMod up (hooks 7/7)"
Меню: плашка RAGEMOD в правом верхнем углу.
NOTE
