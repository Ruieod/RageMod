#!/usr/bin/env python3
"""Бинарник -> C-массив. Используется CMake для вшивания шрифта меню."""
import sys

if len(sys.argv) != 4:
    raise SystemExit('usage: bin2header.py <in> <out.h> <symbol>')

src, dst, sym = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(src, 'rb').read()

with open(dst, 'w', encoding='utf-8') as f:
    f.write('// сгенерировано tools/bin2header.py, править не надо\n#pragma once\n\n')
    f.write(f'static const unsigned int {sym}_size = {len(data)};\n')
    f.write(f'static const unsigned char {sym}[] = {{\n')
    for i in range(0, len(data), 20):
        f.write('    ' + ','.join(str(b) for b in data[i:i + 20]) + ',\n')
    f.write('};\n')
