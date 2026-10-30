#!/usr/bin/env python3
"""Снимает client_dll-оффсеты статикой прямо из client.dll — без запуска игры.

Нужно, когда Valve выкатила патч, а публичного дампа a2x/cs2-dumper под него ещё
нет. Берём ТЕ ЖЕ сигнатуры, что cs2-dumper гоняет по живому процессу
(src/analysis/offsets.rs), и применяем к файлу на диске. Схему при этом только
СВЕРЯЕМ с src/offsets_gen.h: она лежит в client.dll статически и между мелкими
патчами обычно не двигается — скрипт скажет, если сдвинулась.

Чего НЕ делает: кнопки (buttons::*). Их список строится в рантайме, на диске он
пустой — ловятся только прогоном cs2-dumper по живой cs2.exe.

  python3 tools/static_offsets.py path/to/client.dll          # показать
  python3 tools/static_offsets.py path/to/client.dll --write   # вписать в offsets_gen.h

Зависит от pefile:  pip install pefile
"""
import os
import re
import struct
import sys

import pefile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HDR = os.path.join(HERE, 'src', 'offsets_gen.h')

# схемные поля, которые реально использует код (класс -> поля)
SCHEMA = {
    'CBasePlayerController': ['m_iszPlayerName'],
    'CCSPlayerController': ['m_bPawnIsAlive', 'm_hPlayerPawn'],
    'CGameSceneNode': ['m_bDormant', 'm_vecAbsOrigin'],
    'CGlowProperty': ['m_bGlowing', 'm_glowColorOverride', 'm_iGlowType'],
    'CPlayer_WeaponServices': ['m_hActiveWeapon'],
    'CSkeletonInstance': ['m_modelState'],
    'C_AttributeContainer': ['m_Item'],
    'C_BaseEntity': ['m_fFlags', 'm_iHealth', 'm_iTeamNum', 'm_lifeState',
                     'm_pGameSceneNode', 'm_vecVelocity'],
    'C_BaseModelEntity': ['m_Glow', 'm_vecViewOffset'],
    'C_BasePlayerPawn': ['m_pWeaponServices'],
    'C_CSPlayerPawn': ['m_ArmorValue', 'm_bIsScoped', 'm_entitySpottedState',
                       'm_iIDEntIndex', 'm_iShotsFired'],
    'C_EconEntity': ['m_AttributeManager', 'm_flFallbackWear', 'm_nFallbackPaintKit',
                     'm_nFallbackSeed', 'm_nFallbackStatTrak'],
    'C_EconItemView': ['m_bInitialized'],
    'EntitySpottedState_t': ['m_bSpotted'],
}


class Pe:
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.ib = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.OPTIONAL_HEADER.SizeOfImage
        self.data = self.pe.__data__
        self.sec = []
        for s in self.pe.sections:
            nm = s.Name.rstrip(b'\0').decode('latin1')
            self.sec.append((nm, s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData),
                             s.PointerToRawData, s.SizeOfRawData))

    def _sec(self, rva):
        for e in self.sec:
            if e[1] <= rva < e[1] + e[2]:
                return e
        return None

    def rd(self, rva, n):
        e = self._sec(rva)
        if not e:
            return b''
        return self.data[e[3] + (rva - e[1]): e[3] + (rva - e[1]) + n]

    def i32(self, rva):
        b = self.rd(rva, 4)
        return struct.unpack('<i', b)[0] if len(b) == 4 else None

    def i16(self, rva):
        b = self.rd(rva, 2)
        return struct.unpack('<h', b)[0] if len(b) == 2 else None

    def u64(self, rva):
        b = self.rd(rva, 8)
        return struct.unpack('<Q', b)[0] if len(b) == 8 else None

    def v2rva(self, v):
        return v - self.ib if v is not None and self.ib <= v < self.ib + self.img else None

    def cstr(self, rva, m=128):
        b = self.rd(rva, m)
        i = b.find(b'\0')
        return b[:i].decode('latin1', 'replace') if i >= 0 else None

    def blob(self, name):
        for nm, va, sz, off, raw in self.sec:
            if nm == name:
                return va, self.data[off:off + raw]
        return None

    def text(self):
        return self.blob('.text')


# ---- сигнатуры глобалов (as in cs2-dumper src/analysis/offsets.rs) ----
# каждая запись: байты с None-вайлдкардами, смещение disp32 в инструкции, длина инструкции
def _h(s):
    return [None if t in ('?', '??') else int(t, 16) for t in s.split()]


def _scan(tv, tb, sig):
    fixed = [(i, b) for i, b in enumerate(sig) if b is not None]
    first, fb = fixed[0]
    res, i, n = [], 0, len(sig)
    while True:
        j = tb.find(bytes([fb]), i)
        if j < 0:
            break
        st = j - first
        if st >= 0 and st + n <= len(tb) and all(tb[st + k] == b for k, b in fixed):
            res.append(tv + st)
        i = j + 1
    return res


def globals_from(pe):
    tv, tb = pe.text()
    g = {}

    def rip(insn_rva, disp_at, length):
        d = pe.i32(insn_rva + disp_at)
        return insn_rva + length + d

    def one(sig, disp_at, length, want_sect=('.data', '.rdata', '.bss')):
        hits = _scan(tv, tb, _h(sig))
        best = None
        for hh in hits:
            t = rip(hh, disp_at, length)
            e = pe._sec(t)
            if e and e[0] in want_sect:
                if best is None:
                    best = t
                elif best != t:
                    return None  # ambiguous
        return best

    g['dwViewMatrix'] = one('48 8d 0d ?? ?? ?? ?? 48 c1 e0 06', 3, 7)
    g['dwLocalPlayerController'] = one('48 8b 05 ?? ?? ?? ?? 41 89 be', 3, 7)
    g['dwEntityList'] = one('48 89 0d ?? ?? ?? ?? e9 ?? ?? ?? ?? cc', 3, 7)
    g['dwGameEntitySystem'] = one('48 8b 1d ?? ?? ?? ?? 48 89 1d ?? ?? ?? ?? 4c 63 b3', 3, 7)

    h = _scan(tv, tb, _h('ff 81 ?? ?? ?? ?? 48 85 d2'))
    g['dwGameEntitySystem_highestEntityIndex'] = pe.i32(h[0] + 2) if h else None

    csgo = one('48 89 05 ?? ?? ?? ?? 0f 57 c0 0f 11 05', 3, 7)
    g['dwCSGOInput'] = csgo
    h = _scan(tv, tb, _h('f2 42 0f 10 84 28 ?? ?? ?? ??'))
    g['dwViewAngles'] = (csgo + pe.i32(h[0] + 6)) if (h and csgo is not None) else None

    pred = None
    for hh in _scan(tv, tb, _h('48 8d 05 ?? ?? ?? ?? c3')):
        tail = pe.rd(hh + 8, 0x30)
        k = 0
        while k < len(tail) and tail[k] == 0xcc:
            k += 1
        if k >= 8 and tail[k:k + 5] == bytes([0x40, 0x53, 0x56, 0x41, 0x54]):
            t = rip(hh, 3, 7)
            e = pe._sec(t)
            if e and e[0] in ('.data', '.rdata'):
                pred = t
                break
    h = _scan(tv, tb, _h('4c 39 b6 ?? ?? ?? ?? 74 ?? 44 88 be'))
    g['dwLocalPlayerPawn'] = (pred + pe.i32(h[0] + 3)) if (h and pred is not None) else None
    return g


def schema_from(pe):
    rdata = pe.blob('.rdata')
    dat = pe.blob('.data')

    def find_cstr(name):
        res = []
        for va, b in (rdata, dat):
            nd = b'\x00' + name.encode() + b'\x00'
            i = b.find(nd)
            while i != -1:
                res.append(va + i + 1)
                i = b.find(nd, i + 1)
        return res

    def dptrs(tva):
        va, b = dat
        res, nd, i = [], struct.pack('<Q', tva), None
        i = b.find(nd)
        while i != -1:
            res.append(va + i)
            i = b.find(nd, i + 1)
        return res

    def binding(cls):
        for S in find_cstr(cls):
            tva = pe.ib + S
            for P in dptrs(tva):
                b = P - 0x08
                if pe.u64(b + 0x08) != tva:
                    continue
                fc = pe.i16(b + 0x24)
                size = pe.i32(b + 0x20)
                fva = pe.v2rva(pe.u64(b + 0x30))
                if fc is None or not (0 < fc < 4000):
                    continue
                if size is None or not (0 < size < 0x200000) or fva is None:
                    continue
                fn0 = pe.cstr(pe.v2rva(pe.u64(fva)) or 0, 64)
                if not fn0 or not fn0.startswith('m_'):
                    continue
                return fva, fc
        return None

    out = {}
    for cls, fields in SCHEMA.items():
        b = binding(cls)
        out[cls] = {}
        if not b:
            for f in fields:
                out[cls][f] = None
            continue
        fva, fc = b
        fmap = {}
        for i in range(fc):
            e = fva + i * 0x20
            np_ = pe.v2rva(pe.u64(e))
            if np_ is None:
                continue
            nm = pe.cstr(np_, 96)
            if nm:
                fmap[nm] = pe.i32(e + 0x10)
        for f in fields:
            out[cls][f] = fmap.get(f)
    return out


def parse_header():
    old = {}
    if not os.path.exists(HDR):
        return old
    cur = None
    for ln in open(HDR, encoding='utf-8'):
        m = re.match(r'\s*namespace (\w+) \{', ln)
        if m:
            cur = m.group(1)
            old.setdefault(cur, {})
            continue
        m = re.match(r'\s*constexpr uintptr_t (\w+) = (0x[0-9a-fA-F]+);', ln)
        if m and cur:
            old[cur][m.group(1)] = int(m.group(2), 16)
    return old


GLOBAL_ORDER = ['dwEntityList', 'dwLocalPlayerController', 'dwLocalPlayerPawn', 'dwViewMatrix',
                'dwViewAngles', 'dwCSGOInput', 'dwGameEntitySystem',
                'dwGameEntitySystem_highestEntityIndex']


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    write = '--write' in sys.argv
    if not args:
        print(__doc__)
        raise SystemExit(2)
    pe = Pe(args[0])
    ts = pe.pe.FILE_HEADER.TimeDateStamp
    print(f'client.dll ImageBase={hex(pe.ib)} PE TimeDateStamp={hex(ts)}')

    old = parse_header()
    g = globals_from(pe)
    print('\n== client_dll globals ==')
    bad = []
    for k in GLOBAL_ORDER:
        v = g.get(k)
        o = old.get('client_dll', {}).get(k)
        tag = 'MISS' if v is None else ('same' if v == o else f'(was {hex(o)})' if o else 'new')
        print(f'  {k} = {hex(v) if v is not None else None}  {tag}')
        if v is None:
            bad.append(k)

    sch = schema_from(pe)
    ok = chg = miss = 0
    changed = []
    for cls, fm in sch.items():
        for f, v in fm.items():
            o = old.get(cls, {}).get(f)
            if v is None:
                miss += 1
            elif o is None:
                pass
            elif v == o:
                ok += 1
            else:
                chg += 1
                changed.append(f'{cls}::{f} {hex(o)}->{hex(v)}')
    print(f'\n== schema check vs offsets_gen.h ==\n  unchanged={ok} changed={chg} unreadable={miss}')
    for c in changed:
        print('  CHANGED', c)
    if chg:
        print('  !! схема сдвинулась — offsets_gen.h по схеме пора обновлять с живого дампа')

    if write and not bad:
        txt = open(HDR, encoding='utf-8').read()
        for k in GLOBAL_ORDER:
            if g.get(k) is None:
                continue
            txt = re.sub(rf'(constexpr uintptr_t {k} = )0x[0-9a-fA-F]+;',
                         rf'\g<1>{hex(g[k])};', txt)
        open(HDR, 'w', encoding='utf-8').write(txt)
        print(f'\nзаписал {len(GLOBAL_ORDER)} глобалов в {HDR}')
    elif write and bad:
        print(f'\nНЕ пишу: не снялись {bad} (сигнатуры устарели — сверь src/analysis/offsets.rs у a2x)')
    print('\nbuttons::* статикой не берутся (рантайм-список) — для auto-fire/trigger/bhop '
          'нужен прогон cs2-dumper по живой игре.')


if __name__ == '__main__':
    main()
