#!/usr/bin/env python3
"""Вставляет System.loadLibrary("ragemod") в статический конструктор точки входа apk.

Запускается по каталогу, который распаковал apktool. Цель выбирается так:
application android:name из манифеста, а если своего Application нет — launcher-активити.
Идемпотентно: второй запуск ничего не меняет.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

ANDROID = '{http://schemas.android.com/apk/res/android}'
LIB = 'ragemod'

CLINIT = f'''.method static constructor <clinit>()V
    .locals 1

    const-string v0, "{LIB}"

    invoke-static {{v0}}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    return-void
.end method
'''

LOAD_BODY = f'''
    const-string v0, "{LIB}"

    invoke-static {{v0}}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
'''


def entry_class(decoded):
    tree = ET.parse(os.path.join(decoded, 'AndroidManifest.xml'))
    app = tree.getroot().find('application')
    if app is None:
        raise SystemExit('в манифесте нет <application>')

    name = app.get(ANDROID + 'name')
    if name and not name.startswith('android.app.'):
        return name

    for activity in app.iter('activity'):
        for f in activity.iter('intent-filter'):
            actions = [a.get(ANDROID + 'name') for a in f.iter('action')]
            cats = [c.get(ANDROID + 'name') for c in f.iter('category')]
            if 'android.intent.action.MAIN' in actions and 'android.intent.category.LAUNCHER' in cats:
                return activity.get(ANDROID + 'name')
    raise SystemExit('не нашли ни своего Application, ни launcher-активити')


def smali_path(decoded, cls):
    rel = cls.lstrip('.').replace('.', '/') + '.smali'
    for d in sorted(os.listdir(decoded)):
        if not d.startswith('smali'):
            continue
        p = os.path.join(decoded, d, rel)
        if os.path.isfile(p):
            return p
    raise SystemExit(f'нет smali для {cls} (искали {rel})')


def bump_locals(line):
    m = re.match(r'(\s*\.(?:locals|registers)\s+)(\d+)', line)
    if not m:
        return line, False
    n = int(m.group(2))
    return (m.group(1) + str(max(n, 1)) + '\n', True)


def patch(path):
    src = open(path, encoding='utf-8').read()
    if LIB in src:
        print(f'уже пропатчен: {path}')
        return

    lines = src.splitlines(keepends=True)
    out = []
    i = 0
    done = False
    while i < len(lines):
        out.append(lines[i])
        if not done and lines[i].startswith('.method') and '<clinit>()V' in lines[i]:
            i += 1
            # сразу за .locals/.registers вставляем загрузку: v0 в начале метода свободен
            while i < len(lines):
                fixed, is_decl = bump_locals(lines[i])
                out.append(fixed)
                i += 1
                if is_decl:
                    out.append(LOAD_BODY)
                    done = True
                    break
            continue
        i += 1

    if done:
        open(path, 'w', encoding='utf-8').write(''.join(out))
        print(f'загрузка добавлена в <clinit>: {path}')
        return

    # своего статического конструктора нет — дописываем целиком
    src = src.rstrip('\n') + '\n\n' + CLINIT
    open(path, 'w', encoding='utf-8').write(src)
    print(f'<clinit> создан: {path}')


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: patch_smali.py <apktool-decoded-dir>')
    decoded = sys.argv[1]
    cls = entry_class(decoded)
    print(f'точка входа: {cls}')
    patch(smali_path(decoded, cls))


if __name__ == '__main__':
    main()
