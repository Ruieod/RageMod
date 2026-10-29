#!/usr/bin/env python3
"""Тянет свежие оффсеты CS2 из cs2-dumper и генерит src/offsets_gen.h.

Берём только то, что реально используем. Если Valve что-то переименовала и поля
нет — скрипт падает с именем поля, а не молча кладёт ноль в бинарь.

  python3 tools/update_offsets.py            # скачать и сгенерить
  python3 tools/update_offsets.py --offline  # пересобрать из work/cs2-dumper
"""
import os
import re
import sys
import urllib.request

BASE = 'https://raw.githubusercontent.com/a2x/cs2-dumper/main/output'
FILES = ['offsets.hpp', 'buttons.hpp', 'client_dll.hpp', 'engine2_dll.hpp']
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(HERE, 'work', 'cs2-dumper')
OUT = os.path.join(HERE, 'src', 'offsets_gen.h')

# что берём: модуль/класс -> поля
WANT_GLOBAL = {
    'client_dll': ['dwEntityList', 'dwLocalPlayerController', 'dwLocalPlayerPawn', 'dwViewMatrix',
                   'dwViewAngles', 'dwCSGOInput', 'dwGlobalVars', 'dwGameRules', 'dwPlantedC4',
                   'dwSensitivity', 'dwSensitivity_sensitivity', 'dwGameEntitySystem',
                   'dwGameEntitySystem_highestEntityIndex', 'dwGlowManager'],
    'engine2_dll': ['dwNetworkGameClient', 'dwNetworkGameClient_localPlayer',
                    'dwNetworkGameClient_maxClients', 'dwNetworkGameClient_signOnState',
                    'dwBuildNumber', 'dwWindowHeight', 'dwWindowWidth'],
}
WANT_BUTTONS = ['attack', 'attack2', 'jump', 'duck', 'reload']
WANT_SCHEMA = {
    'C_BaseEntity': ['m_iHealth', 'm_iTeamNum', 'm_pGameSceneNode', 'm_lifeState', 'm_fFlags', 'm_vecVelocity'],
    'CGameSceneNode': ['m_vecAbsOrigin', 'm_bDormant'],
    'CSkeletonInstance': ['m_modelState'],
    'CBasePlayerController': ['m_hPawn', 'm_iszPlayerName'],
    'CCSPlayerController': ['m_hPlayerPawn', 'm_bPawnIsAlive', 'm_iPawnHealth', 'm_sSanitizedPlayerName',
                            'm_iPawnArmor', 'm_bPawnHasHelmet'],
    'C_CSPlayerPawnBase': ['m_flFlashDuration', 'm_flFlashMaxAlpha'],
    'C_CSPlayerPawn': ['m_angEyeAngles', 'm_iShotsFired', 'm_ArmorValue', 'm_bIsScoped',
                       'm_bIsDefusing', 'm_aimPunchAngle', 'm_aimPunchCache', 'm_bIsWalking',
                       'm_iIDEntIndex', 'm_entitySpottedState'],
    'C_BasePlayerPawn': ['m_pWeaponServices', 'm_pMovementServices'],
    'CPlayer_WeaponServices': ['m_hActiveWeapon'],
    'C_BaseModelEntity': ['m_Glow', 'm_clrRender', 'm_vecViewOffset'],
    'CGlowProperty': ['m_glowColorOverride', 'm_bGlowing', 'm_iGlowType', 'm_iGlowTeam'],
    'EntitySpottedState_t': ['m_bSpotted', 'm_bSpottedByMask'],
    'C_BaseFlex': ['m_flexWeight'],
    'C_EconEntity': ['m_AttributeManager', 'm_bClientSideWeaponFlipped', 'm_nFallbackPaintKit',
                     'm_flFallbackWear', 'm_nFallbackSeed', 'm_nFallbackStatTrak'],
    'C_AttributeContainer': ['m_Item'],
    'C_EconItemView': ['m_iItemDefinitionIndex', 'm_iEntityQuality', 'm_bInitialized', 'm_iAccountID'],
    'C_BasePlayerWeapon': ['m_nNextPrimaryAttackTick', 'm_iClip1', 'm_iClip2'],
    'C_CSWeaponBase': ['m_bUIWeapon'],
    'C_BaseCombatCharacter': ['m_pVecAttributes'],
    'C_PlantedC4': ['m_bBombTicking', 'm_flC4Blow', 'm_bBeingDefused'],
}

OPTIONAL = {'m_aimPunchAngle', 'm_aimPunchCache', 'm_vecLastClipCameraPos', 'm_bUIWeapon',
            'm_iGlowTeam', 'm_flexWeight', 'm_pVecAttributes', 'm_iAccountID',
            'dwWindowHeight', 'dwWindowWidth', 'dwNetworkGameClient_signOnState',
            'm_bClientSideWeaponFlipped', 'm_iPawnArmor', 'm_bPawnHasHelmet', 'm_iClip2'}


def fetch(offline):
    os.makedirs(CACHE, exist_ok=True)
    out = {}
    for f in FILES:
        p = os.path.join(CACHE, f)
        if not offline:
            with urllib.request.urlopen(f'{BASE}/{f}', timeout=60) as r:
                data = r.read().decode('utf-8', 'replace')
            open(p, 'w', encoding='utf-8').write(data)
        out[f] = open(p, encoding='utf-8').read()
    return out


def parse_namespaces(text):
    """namespace X { ... } -> {X: {поле: значение}}"""
    res = {}
    cur = None
    depth_names = []
    for line in text.split('\n'):
        m = re.match(r'\s*namespace (\w+) \{', line)
        if m:
            depth_names.append(m.group(1))
            cur = m.group(1)
            res.setdefault(cur, {})
            continue
        if re.match(r'\s*\}', line):
            if depth_names:
                depth_names.pop()
            cur = depth_names[-1] if depth_names else None
            continue
        m = re.match(r'\s*constexpr std::ptrdiff_t (\w+) = (0x[0-9A-Fa-f]+|\d+);', line)
        if m and cur:
            res[cur][m.group(1)] = int(m.group(2), 0)
    return res


def main():
    offline = '--offline' in sys.argv
    src = fetch(offline)
    stamp = ''
    m = re.search(r'// (\d{4}-\d{2}-\d{2} [\d:.]+ UTC)', src['offsets.hpp'])
    if m:
        stamp = m.group(1)

    g = parse_namespaces(src['offsets.hpp'])
    b = parse_namespaces(src['buttons.hpp'])
    s = parse_namespaces(src['client_dll.hpp'])

    missing = []
    lines = ['// сгенерировано tools/update_offsets.py из github.com/a2x/cs2-dumper',
             f'// дамп от {stamp}', '#pragma once', '#include <cstdint>', '', 'namespace off {']

    for mod, names in WANT_GLOBAL.items():
        lines.append(f'namespace {mod} {{')
        for n in names:
            v = g.get(mod, {}).get(n)
            if v is None:
                (missing.append(f'{mod}::{n}') if n not in OPTIONAL else None)
                continue
            lines.append(f'constexpr uintptr_t {n} = {hex(v)};')
        lines.append('}')

    lines.append('namespace buttons {')
    for n in WANT_BUTTONS:
        v = b.get('buttons', {}).get(n)
        if v is None:
            missing.append(f'buttons::{n}')
            continue
        lines.append(f'constexpr uintptr_t {n} = {hex(v)};')
    lines.append('}')

    for cls, names in WANT_SCHEMA.items():
        lines.append(f'namespace {cls} {{')
        for n in names:
            v = s.get(cls, {}).get(n)
            if v is None:
                (missing.append(f'{cls}::{n}') if n not in OPTIONAL else None)
                continue
            lines.append(f'constexpr uintptr_t {n} = {hex(v)};')
        lines.append('}')
    lines.append('} // namespace off')

    if missing:
        print('НЕ НАЙДЕНО (Valve переименовала или дампер отстал):', file=sys.stderr)
        for x in missing:
            print('  ' + x, file=sys.stderr)
        raise SystemExit(1)

    open(OUT, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print(f'{OUT}: {sum(len(v) for v in WANT_SCHEMA.values())} полей схемы, дамп от {stamp}')


if __name__ == '__main__':
    main()
