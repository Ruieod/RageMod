#include "game.h"

#include <windows.h>

#include <cstring>

#include "antiaim.h"
#include "config.h"
#include "sdk.h"

namespace game {
namespace {

Frame g_frame;

struct Target {
    bool ok = false;
    uintptr_t pawn = 0;
    Vec3 point;
    float fov = 0.f;
};
Target g_target;

// здоровье врагов с прошлого кадра: по его просадке ловим попадания
struct HpTrack {
    uintptr_t pawn = 0;
    int hp = 0;
};
HpTrack g_hp[kMaxPlayers];
int g_hp_n = 0;
HitEvent g_hits[kMaxHits];
int g_hits_n = 0;
int g_last_shots = 0;
int64_t g_last_shot_ts = 0;

int64_t now_ms() {
    static LARGE_INTEGER freq{};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000 / freq.QuadPart;
}

// дешёвый xorshift для человеческого разброса; сид от QPC, чтобы профиль не повторялся
float frand(float lo, float hi) {
    static uint32_t s = 0;
    if (!s) { LARGE_INTEGER c{}; QueryPerformanceCounter(&c); s = (uint32_t)c.QuadPart | 1u; }
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return lo + (hi - lo) * ((float)(s & 0xFFFFFFu) / (float)0xFFFFFFu);
}

} // namespace

// скелет: пары костей, рисует ui
const BoneLink kBoneLinks[] = {
    {sdk::BoneHead, sdk::BoneNeck},         {sdk::BoneNeck, sdk::BoneSpine1},
    {sdk::BoneSpine1, sdk::BonePelvis},     {sdk::BoneNeck, sdk::BoneLeftShoulder},
    {sdk::BoneLeftShoulder, sdk::BoneLeftElbow}, {sdk::BoneLeftElbow, sdk::BoneLeftHand},
    {sdk::BoneNeck, sdk::BoneRightShoulder}, {sdk::BoneRightShoulder, sdk::BoneRightElbow},
    {sdk::BoneRightElbow, sdk::BoneRightHand}, {sdk::BonePelvis, sdk::BoneLeftHip},
    {sdk::BoneLeftHip, sdk::BoneLeftKnee},  {sdk::BoneLeftKnee, sdk::BoneLeftFoot},
    {sdk::BonePelvis, sdk::BoneRightHip},   {sdk::BoneRightHip, sdk::BoneRightKnee},
    {sdk::BoneRightKnee, sdk::BoneRightFoot},
};
const int kBoneLinkCount = (int)(sizeof(kBoneLinks) / sizeof(kBoneLinks[0]));

namespace {

const int kBoneIds[] = {sdk::BoneHead,  sdk::BoneNeck,          sdk::BoneSpine1,       sdk::BonePelvis,
                        sdk::BoneLeftShoulder, sdk::BoneLeftElbow, sdk::BoneLeftHand,  sdk::BoneRightShoulder,
                        sdk::BoneRightElbow,   sdk::BoneRightHand, sdk::BoneLeftHip,   sdk::BoneLeftKnee,
                        sdk::BoneLeftFoot,     sdk::BoneRightHip,  sdk::BoneRightKnee, sdk::BoneRightFoot};

bool spotted(sdk::Entity e) {
    // m_bSpotted в EntitySpottedState_t: игра сама считает видимость для радара
    uintptr_t st = e.ptr + off::C_CSPlayerPawn::m_entitySpottedState;
    return mem::read<bool>(st + off::EntitySpottedState_t::m_bSpotted);
}

void apply_glow(sdk::Entity e, bool enemy, bool visible) {
    if (!g_cfg.glow) return;
    if (g_cfg.esp_team_check && !enemy) return;
    const float* c = (g_cfg.glow_split_visible && visible) ? g_cfg.glow_color_visible : g_cfg.glow_color;
    uint8_t r = (uint8_t)(c[0] * 255.f);
    uint8_t g = (uint8_t)(c[1] * 255.f);
    uint8_t b = (uint8_t)(c[2] * 255.f);
    uint32_t col = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (0xFFu << 24);
    uintptr_t glow = e.ptr + off::C_BaseModelEntity::m_Glow;
    mem::write<uint32_t>(glow + off::CGlowProperty::m_glowColorOverride, col);
    mem::write<int>(glow + off::CGlowProperty::m_iGlowType, g_cfg.glow_type);
    mem::write<bool>(glow + off::CGlowProperty::m_bGlowing, true);
}

void apply_skin(sdk::Entity local) {
    if (!g_cfg.skins) return;
    uintptr_t w = local.active_weapon();
    if (!w) return;
    mem::write<int>(w + off::C_EconEntity::m_nFallbackPaintKit, g_cfg.skin_paintkit);
    mem::write<int>(w + off::C_EconEntity::m_nFallbackSeed, g_cfg.skin_seed);
    mem::write<float>(w + off::C_EconEntity::m_flFallbackWear, g_cfg.skin_wear);
    mem::write<int>(w + off::C_EconEntity::m_nFallbackStatTrak, g_cfg.skin_stattrak);
    uintptr_t item = w + off::C_EconEntity::m_AttributeManager + off::C_AttributeContainer::m_Item;
    mem::write<bool>(item + off::C_EconItemView::m_bInitialized, true);
}

Vec3 aim_point(sdk::Entity e) {
    Vec3 p = e.bone(g_cfg.aim_bone);
    if (p.bad() || (p.x == 0.f && p.y == 0.f && p.z == 0.f)) p = e.origin() + Vec3{0.f, 0.f, 64.f};
    if (g_cfg.aim_predict) {
        Vec3 vel = mem::read<Vec3>(e.ptr + off::C_BaseEntity::m_vecVelocity);
        if (!vel.bad()) {
            // упреждение на один тик сети плюс настроенный множитель
            float t = (1.f / 64.f) * g_cfg.aim_predict_scale;
            p = p + vel * t;
        }
    }
    return p;
}

void run_aim(sdk::Entity local, const Vec3& eye) {
    static Vec3 saved{};
    static int restore_frames = 0;
    static uintptr_t locked = 0;              // по какой цели уже отрабатываем реакцию
    static int64_t ready_ms = 0;              // когда разрешён следующий снап-выстрел

    if (restore_frames > 0 && --restore_frames == 0) {
        sdk::set_view_angles(saved);          // вернули угол после выстрела
        sdk::press_attack(false);
    }

    bool on = config::bind_active(g_cfg.aim_bind, g_cfg.aim);
    if (!on || !g_target.ok) { locked = 0; return; }   // нет цели — реакция перевзводится

    // при AA в real-режиме текущий угол — фейковый; снап делаем от него, но
    // возвращать после выстрела надо туда же, иначе спин собьётся
    Vec3 want = calc_angle(eye, g_target.point);
    Vec3 cur = sdk::view_angles();

    // человеческий увод: не садимся в центр кости ровно, VACnet ловит идеальный угол
    if (g_cfg.match_safe && g_cfg.aim_humanize > 0.f) {
        const float h = g_cfg.aim_humanize;
        want.x = norm_angle(want.x + frand(-h, h));
        want.y = norm_angle(want.y + frand(-h, h));
    }

    // задержка реакции по новой цели + разрыв между выстрелами, с джиттером
    bool fire_gate = true;
    if (g_cfg.match_safe) {
        const int64_t t = now_ms();
        if (locked != g_target.pawn) {
            locked = g_target.pawn;
            ready_ms = t + g_cfg.aim_reaction_ms + (int64_t)frand(0.f, g_cfg.aim_reaction_ms * 0.5f);
        }
        if (t < ready_ms) fire_gate = false;
    }

    if (g_cfg.aim_silent && g_cfg.aim_auto_fire) {
        if (!fire_gate) return;               // ещё «не успели» среагировать — не стреляем
        saved = cur;
        sdk::set_view_angles(want);
        sdk::press_attack(true);
        restore_frames = 2;                   // кадр на выстрел, потом откат
        if (g_cfg.match_safe)
            ready_ms = now_ms() + g_cfg.aim_refire_ms + (int64_t)frand(0.f, (float)g_cfg.aim_refire_ms);
        return;
    }

    float smooth = g_cfg.aim_smooth < 1.f ? 1.f : g_cfg.aim_smooth;
    Vec3 step{cur.x + norm_angle(want.x - cur.x) / smooth, cur.y + norm_angle(want.y - cur.y) / smooth, 0.f};
    sdk::set_view_angles(step);
    if (g_cfg.aim_auto_fire && g_target.fov < g_cfg.aim_fov * 0.3f && fire_gate) {
        sdk::press_attack(true);
        if (g_cfg.match_safe)
            ready_ms = now_ms() + g_cfg.aim_refire_ms + (int64_t)frand(0.f, (float)g_cfg.aim_refire_ms);
    }
}

void press_jump(bool down) {
    mem::write<int>(sdk::g_client + off::buttons::jump + 0x8, down ? 0x5 : 0x4);
}

// Бхоп: прыжок отпускаем в воздухе и жмём ровно в кадр касания земли.
// Ничего нового в процесс не тащим — пишем ту же структуру кнопки, что и триггер.
void run_bhop(sdk::Entity local) {
    static bool held = false;
    bool on = config::bind_active(g_cfg.bhop_bind, g_cfg.bhop);
    if (!on) {
        if (held) {
            press_jump(false);
            held = false;
        }
        return;
    }
    uint32_t flags = mem::read<uint32_t>(local.ptr + off::C_BaseEntity::m_fFlags);
    bool on_ground = (flags & (1u << 0)) != 0;
    press_jump(on_ground);
    held = true;
}

// Попадания считаем по просадке HP врагов, авторство — по своему счётчику выстрелов.
void track_hits(sdk::Entity local, const Frame& f) {
    int shots = mem::read<int>(local.ptr + off::C_CSPlayerPawn::m_iShotsFired);
    int64_t now = now_ms();
    if (shots != g_last_shots) {
        g_last_shots = shots;
        g_last_shot_ts = now;
    }

    HpTrack next[kMaxPlayers];
    int next_n = 0;
    for (int i = 0; i < f.count && next_n < kMaxPlayers; i++) {
        const PlayerView& p = f.players[i];
        if (!p.enemy) continue;
        int prev = -1;
        for (int k = 0; k < g_hp_n; k++) {
            if (g_hp[k].pawn == p.pawn) {
                prev = g_hp[k].hp;
                break;
            }
        }
        // стреляли меньше секунды назад — считаем урон своим
        if (prev > 0 && p.hp < prev && now - g_last_shot_ts < 1000) {
            HitEvent e;
            e.ts = now;
            e.damage = prev - p.hp;
            e.kill = p.hp <= 0;
            e.screen = p.on_screen ? p.head : Vec2{};
            std::memcpy(e.name, p.name, sizeof(e.name));
            if (g_hits_n < kMaxHits) {
                g_hits[g_hits_n++] = e;
            } else {
                std::memmove(&g_hits[0], &g_hits[1], sizeof(HitEvent) * (kMaxHits - 1));
                g_hits[kMaxHits - 1] = e;
            }
        }
        next[next_n].pawn = p.pawn;
        next[next_n].hp = p.hp;
        next_n++;
    }
    std::memcpy(g_hp, next, sizeof(HpTrack) * (size_t)next_n);
    g_hp_n = next_n;
}

void run_trigger(sdk::Entity local) {
    static int64_t fire_at = 0;
    static bool holding = false;

    bool on = config::bind_active(g_cfg.trigger_bind, g_cfg.trigger);
    if (!on) {
        if (holding) {
            sdk::press_attack(false);
            holding = false;
        }
        fire_at = 0;
        return;
    }
    if (g_cfg.trigger_scoped_only && !mem::read<bool>(local.ptr + off::C_CSPlayerPawn::m_bIsScoped)) return;

    int id = mem::read<int>(local.ptr + off::C_CSPlayerPawn::m_iIDEntIndex, -1);
    bool valid = false;
    if (id > 0 && id < 64) {
        sdk::Entity e{sdk::Entity::entity_by_index(id)};
        if (e && e.alive() && !e.dormant()) {
            bool enemy = e.team() != local.team();
            valid = enemy || !g_cfg.trigger_team_check;
        }
    }

    int64_t t = now_ms();
    if (valid) {
        if (fire_at == 0) fire_at = t + g_cfg.trigger_delay_ms;
        if (t >= fire_at && !holding) {
            sdk::press_attack(true);
            holding = true;
        }
    } else {
        fire_at = 0;
        if (holding) {
            sdk::press_attack(false);
            holding = false;
        }
    }
}

} // namespace

const Frame& frame() { return g_frame; }

void tick(int sw, int sh) {
    Frame f;
    f.screen_w = sw;
    f.screen_h = sh;

    sdk::Controller local_ctrl = sdk::local_controller();
    sdk::Entity local = sdk::local_pawn();
    if (!local_ctrl || !local || !local.ptr) {
        g_target = {};
        g_frame = f;
        return;
    }
    f.in_game = true;

    Vec3 eye = local.eye();
    Vec3 angles = sdk::view_angles();
    uint8_t my_team = local.team();

    Target best;
    best.fov = 1e9f;

    int high = sdk::highest_index();
    for (int i = 1; i <= high && f.count < kMaxPlayers; i++) {
        uintptr_t cp = sdk::Entity::entity_by_index(i);
        if (!cp) continue;
        sdk::Controller ctrl{cp};
        uint32_t h = ctrl.pawn_handle();
        if (h == 0 || h == 0xFFFFFFFF) continue;
        sdk::Entity pawn{sdk::Entity::entity_by_handle(h)};
        if (!pawn || pawn.ptr == local.ptr) continue;
        if (!pawn.alive() || pawn.dormant()) continue;

        PlayerView p;
        p.pawn = pawn.ptr;
        p.enemy = pawn.team() != my_team;
        p.hp = pawn.health();
        p.armor = mem::read<int>(pawn.ptr + off::C_CSPlayerPawn::m_ArmorValue);
        p.visible = spotted(pawn);
        ctrl.name(p.name, sizeof(p.name));

        Vec3 origin = pawn.origin();
        p.dist = (origin - eye).len() * 0.0254f;   // юниты -> метры

        apply_glow(pawn, p.enemy, p.visible);

        // кости и бокс
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
        int shown = 0;
        for (int b = 0; b < (int)(sizeof(kBoneIds) / sizeof(kBoneIds[0])) && p.bone_count < kMaxBones; b++) {
            Vec3 w = pawn.bone(kBoneIds[b]);
            if (w.bad()) continue;
            Vec2 s;
            if (!sdk::world_to_screen(w, &s, sw, sh)) continue;
            p.bones[p.bone_count].bone = kBoneIds[b];
            p.bones[p.bone_count].screen = s;
            p.bones[p.bone_count].on = true;
            p.bone_count++;
            minx = s.x < minx ? s.x : minx;
            maxx = s.x > maxx ? s.x : maxx;
            miny = s.y < miny ? s.y : miny;
            maxy = s.y > maxy ? s.y : maxy;
            shown++;
        }

        Vec2 sf{}, sh2{};
        bool okf = sdk::world_to_screen(origin, &sf, sw, sh);
        bool okh = sdk::world_to_screen(origin + Vec3{0.f, 0.f, 72.f}, &sh2, sw, sh);
        if (okf && okh) {
            p.feet = sf;
            p.head = sh2;
            p.box_w = (sf.y - sh2.y) * 0.45f;
            p.on_screen = true;
        } else if (shown >= 2) {
            p.feet = {(minx + maxx) * 0.5f, maxy + 4.f};
            p.head = {(minx + maxx) * 0.5f, miny - 6.f};
            p.box_w = (maxx - minx) + 8.f;
            p.on_screen = true;
        }

        // цель для аима
        if (p.enemy && pawn.alive()) {
            Vec3 point = aim_point(pawn);
            Vec3 want = calc_angle(eye, point);
            float d = angle_diff(want, angles);
            bool vis_ok = p.visible || (!g_cfg.aim_visible_only && !g_cfg.match_safe);
            if (d <= g_cfg.aim_fov && vis_ok && d < best.fov) {
                best.ok = true;
                best.pawn = pawn.ptr;
                best.point = point;
                best.fov = d;
            }
        }

        f.players[f.count++] = p;
    }

    g_target = best;
    if (best.ok) {
        Vec2 s;
        if (sdk::world_to_screen(best.point, &s, sw, sh)) {
            f.has_target = true;
            f.target_screen = s;
        }
    }

    // AA первым: аим на выстреле перезапишет угол поверх него и сам же вернёт
    antiaim::run(eye, best.ok, best.point);
    run_aim(local, eye);
    run_trigger(local);
    run_bhop(local);
    apply_skin(local);
    track_hits(local, f);
    // отдаём только свежие события, старые чистим здесь же
    f.hit_count = 0;
    int keep = 0;
    for (int i = 0; i < g_hits_n; i++) {
        if (now_ms() - g_hits[i].ts > 2500) continue;
        g_hits[keep++] = g_hits[i];
        if (f.hit_count < kMaxHits) f.hits[f.hit_count++] = g_hits[i];
    }
    g_hits_n = keep;

    g_frame = f;
}

} // namespace game
