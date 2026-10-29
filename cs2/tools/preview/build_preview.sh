#!/usr/bin/env bash
# Рендерит меню CS2 на хосте в png: ./build_preview.sh [tab] [w] [h]
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
IMGUI=$ROOT/../standoff2/third_party/imgui
OUT=/tmp/rm-cs2-preview
mkdir -p $OUT
TAB="${1:-0}"; W="${2:-1920}"; H="${3:-1080}"

python3 $ROOT/../standoff2/tools/bin2header.py $ROOT/../standoff2/third_party/font/Roboto-Medium.ttf \
  $OUT/roboto_font.h kFontRoboto

rm -f $OUT/preview                 # иначе при ошибке компиляции тихо запустится старый бинарь
clang++ -std=c++17 -O1 -fno-exceptions -fno-rtti -DRM_PREVIEW \
  -I$ROOT/src -I$IMGUI -I$OUT \
  preview_main.cpp $ROOT/src/ui.cpp \
  $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
  -o $OUT/preview || { echo "сборка превью не удалась"; exit 1; }

$OUT/preview "$W" "$H" "$TAB" "$OUT/preview.ppm"
python3 - "$OUT/preview.ppm" "$OUT/cs2-menu-$TAB.png" <<'PY'
import sys, zlib, struct
src, dst = sys.argv[1], sys.argv[2]
d = open(src,'rb').read()
parts = d.split(b'\n', 3)
w, h = map(int, parts[1].split())
raw = parts[3]
rows = b''.join(b'\x00' + raw[y*w*3:(y+1)*w*3] for y in range(h))
def chunk(t, data):
    c = t + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) \
      + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b'')
open(dst,'wb').write(png)
print(dst)
PY
