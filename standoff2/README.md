# RageMod — Standoff 2 (internal, arm64)

Внутренний чит под `com.axlebolt.standoff2` **1.0.0 arm64-v8a** (versionCode 203964).
Собран по дампу этой же сборки: `libunity.so` sha256
`a5d530e0ee7f4982d4bafec50c6109737b6119525935cac3e423e5145ff33e0f`.
Код il2cpp у SO2 слит в `libunity.so`, экспортов `il2cpp_*` нет — поэтому ни одного
рантайм-лукапа класса, всё по RVA и смещениям полей из `src/offsets.h`.

## Фичи

| | |
|---|---|
| ESP | бокс, скелет по `HitBoxCollider[]`, полоска HP, дистанция, снаплайны, фильтры по команде/видимости |
| Silent aim | правится сам хит-каст выстрела, камера не двигается. Два режима, см. ниже |
| Visible aim | пишет сетевые кватернионы `eir` локального игрока, smooth/FOV/бон |
| Auto fire | хук геттера кнопки: держать всегда или только когда цель в конусе, pulse для одиночных |
| No recoil | гасим `RecoilState` в `WeaponContext` после пересчёта, `RecoilMult` слайдером |
| Chams | материалы `OutlineMaterial`/`UntouchableMaterial` из `CharacterBindings` + render queue (видно через стены) |
| Third person | камера отводится назад после кадрового тика, с упором в геометрию, плюс включение своей модели |
| Меню | ImGui поверх `eglSwapBuffers`, вкладки ESP / AIMBOT / FIRE / MISC / CONFIG, конфиг в JSON |

Ничего не включено на старте, кроме ESP.

## Сборка

```bash
ANDROID_NDK=/path/to/android-ndk-r27c ./build.sh          # -> build/arm64/libragemod.so
ANDROID_NDK=/path/to/android-ndk-r27c ./pack_zygisk.sh    # -> dist/ragemod-zygisk.zip
```

## Как запустить

Железо: нужен **arm64 Android**. Либа arm64-v8a, и на BlueStacks Pie64 игра идёт через
ARM-трансляцию — инжект туда отдельный геморрой. Рутованный телефон или arm64-образ
эмулятора (AVD arm64, Waydroid arm64) проще на порядок.

**Путь 1 — Zygisk-модуль (так и задумано)**

1. Magisk 24+, Zygisk включён.
2. `./pack_zygisk.sh` → `dist/ragemod-zygisk.zip`.
3. Magisk → Modules → Install from storage → выбрать zip → reboot.
4. Запустить игру, проверить `adb logcat -s RageMod`: должно быть `RageMod up (hooks 6/6)`.

Та же либа и есть модуль: Magisk грузит её в каждый форк зиготы, в чужих процессах она
сразу просит `DLCLOSE_MODULE_LIBRARY` и ничего не делает, поднимается только в
`com.axlebolt.standoff2`.

**Путь 2 — инжект в живой процесс (root shell)**

```bash
adb push build/arm64/libragemod.so /data/local/tmp/
adb shell su -c 'chmod 644 /data/local/tmp/libragemod.so'
# любым arm64 dlopen-инжектором:
adb shell su -c './inject -p $(pidof com.axlebolt.standoff2) -l /data/local/tmp/libragemod.so'
```

Или через frida:

```bash
frida -U -n com.axlebolt.standoff2 -e 'Module.load("/data/local/tmp/libragemod.so")'
```

Конструктор либы сам сверяет `/proc/self/cmdline` с именем пакета, так что в чужой процесс
её грузить безопасно — просто ничего не произойдёт.

**Путь 3 — без рута, репак apk**

Положить `.so` в `lib/arm64-v8a/`, добавить `System.loadLibrary("ragemod")` в
`UnityPlayerActivity` (smali), подписать, поставить. Без рута не будет `/dev/input`, то есть
меню не нажимается: всё только через файл конфига, а править его без рута неудобно.
Рабочий вариант, но для настройки всё равно нужен рут.

## Управление

- меню: кнопка `RAGE` в левом верхнем углу или **VOLUME DOWN**
- ввод меню читается с `/dev/input` — **нужен root**; без него чит работает, но управляется
  только файлом `/data/data/com.axlebolt.standoff2/files/ragemod.json`
- тач смещён или перевёрнут — вкладка MISC, флаги `Swap XY / Invert X / Invert Y`
- лог: `adb logcat -s RageMod`
- после патча игры хуки не встанут: смотри раздел «Под новый патч игры»

`// у SO2 свой анти-чит: внешнее чтение памяти он переживает, инжект в процесс — риск аккаунта.`

## Точки перехвата

| хук | RVA | зачем |
|---|---|---|
| `ios.lxcq()` | `0xA0903F4` | кадровый тик камеры локального игрока: сбор сущностей, W2S, visible aim |
| `jxy.vrsg(ref WeaponContext)` | `0x973A818` | выстрел — silent режим 0 |
| `jxy.rxjx(ref WeaponContext, Span<Ray>)` | `0x9739FA4` | лучи хит-каста — silent режим 1 |
| `ake.opmn()` | `0x949E8A0` | страховка на список игроков, если реестр мира развалится |
| `znn.rpcp(string)` | `0x8D441BC` | геттер состояния кнопки по имени экшена — автострельба |
| `vez.lhgb(ref WeaponContext, bool)` | `0x9731070` | пересчёт отдачи — no recoil |
| `eglSwapBuffers` | GOT `libunity.so` | рендер меню и ESP |

Прологи всех шести игровых целей в этой сборке PC-независимы (проверено дизасмом),
поэтому inline-хук — простой 16-байтный патч `ldr x16,#8 / br x16/ .quad`, без движка хуков.
Если после патча игры пролог станет PC-зависимым, хук откажется ставиться и напишет в лог,
а не испортит код.

## Цепочки

```
ios  +0x18 -> ake (локальный игрок)       ios +0x20 -> njh (мир)
njh  +0x40 -> gat +0x30 -> gzx +0x18 -> yjk<krm>  (+0x10 массив, +0x18 count)
ake  +0x70 -> ylw (hp 0x10, maxhp 0x14, armor 0x16)   ake +0x90 team   ake +0x28 -> rct -> List<eur>
List<eur> -> элемент с klass->name == "jgi" -> +0x18 HitBoxCollider[]
HitBoxCollider +0x20 HumanBodyBones, Component.get_transform -> позиция бона
ios  +0x38 -> eir (+0x10 quat A, +0x20 quat B)
jgi  +0x30 -> CharacterBindings (+0x38 CharacterRenderer, +0x50 Untouchable, +0x58 Outline)
WeaponContext +0x40 RecoilContext.RecoilMult, +0x68 RecoilState (LastShotTime на +0x84 не трогаем)
```

## Что проверено, а что нет

Проверено по дампу и дизасму:

- смещения полей и RVA — сверены с `dump.cs` и `script.json` дампа 1.0.0;
- `eir.tgfv/rsxh` — тривиальные сеттеры (`stp s0,s1,[x0,#0x10]`), поэтому кватернионы пишем
  прямо в память;
- `WeaponContext` не обфусцирован: `HitTransform` на `+0x10`;
- слот `eglSwapBuffers` есть в `.rela.plt` libunity.so;
- прологи хук-целей;
- `CharacterBindings`, `WeaponContext`, `RecoilState`, `ButtonInput` не обфусцированы — поля и
  их смещения взяты из дампа как есть.

**Не проверял в игре** (дамп — это ELF установленной сборки, не рантайм):

- какой из двух silent-режимов реально переписывает выстрел. Режим 1 (`ray override`) правит
  уже готовые лучи, режим 0 (`hit transform`) доворачивает источник каста — проверяется
  переключателем в меню за один раунд;
- смысл отдельных флагов `qdp` (видимость игрока по мнению игры) — поэтому видимость считаем
  своим `Physics.Linecast`, а не флагом;
- что именно из двух кватернионов `eir` идёт на сервер — в меню выбор `quat A / B / both`;
- порядок осей тача на конкретном девайсе;
- имя input-экшена выстрела. Хук `znn.rpcp` логирует все имена, которые игра реально спрашивала,
  вкладка FIRE показывает их кнопками — ткнул, и автострельба привязалась к нужному;
- как выглядят chams с `OutlineMaterial`/`UntouchableMaterial` на SkinnedMeshRenderer. Если
  получится каша — режим `render queue` без подмены материала даёт просто модель через стены;
- на каком этапе кадра игра пересчитывает позицию камеры: третье лицо пишем после `ios.lxcq`,
  если камера всё равно дёргается — точку перехвата надо сдвинуть на `nrl.LateTick` (`0x98C81B0`).

## Под новый патч игры

1. Перегенерить дамп (`Il2CppDumper`/`Cpp2IL`) по новой `libunity.so`.
2. Обновить `src/offsets.h` — все магические числа только там.
3. Проверить прологи новых хук-целей (`llvm-objdump -d --start-address=...`); если PC-зависимы —
  сдвинуть точку перехвата на вложенный метод, как `iqkr` -> `lxcq`.
