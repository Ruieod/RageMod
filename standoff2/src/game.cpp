#include "game.h"

#include <time.h>
#include <mutex>

#include "config.h"
#include "log.h"

using namespace il2cpp;

namespace game {
namespace {

std::mutex g_mu;
Frame g_frame;

struct {
    void* camera = nullptr;
    Vec3 pos, fwd;
    int w = 0, h = 0;
    float fov = 60.f;
    void* local = nullptr;
    uint8_t team = 0;
    AimTarget target;
} g_live; // состояние текущего кадра, пишется и читается только с игрового треда

// страховка на случай, если цепочка реестра njh->gat->gzx развалится после патча:
// хук ake.opmn() складывает сюда живых игроков
constexpr int kSeenCap = 64;
struct Seen { void* ent; int64_t ts; };
Seen g_seen[kSeenCap];
std::mutex g_seen_mu;

int64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool interesting_bone(int b) {
    if (b == 54) return true;              // UpperChest
    return b >= 0 && b <= 20;              // корпус и конечности, пальцы с лицом не нужны
}

// jgi (модель) игрока: parts -> List<eur> -> элемент с klass->name == "jgi"
void* find_model(void* ent) {
    void* parts = ptr(ent, off::fld::mfc_parts);
    if (!readable(parts, 0x20)) return nullptr;
    void* list = ptr(parts, off::fld::rct_list);
    if (!readable(list, 0x20)) return nullptr;
    void* items = ptr(list, off::il2::List_items);
    int size = read<int>(list, off::il2::List_size);
    if (!readable(items, off::il2::Array_data) || size <= 0 || size > 64) return nullptr;
    void** data = (void**)((char*)items + off::il2::Array_data);
    if (!readable(data, sizeof(void*) * (size_t)size)) return nullptr;
    for (int i = 0; i < size; i++) {
        if (is_class(data[i], off::cls::model)) return data[i];
    }
    return nullptr;
}

int collect_bones(void* model, BoneView* out, int cap) {
    if (!model) return 0;
    void* arr = ptr(model, off::fld::jgi_hitboxes);
    if (!readable(arr, off::il2::Array_data)) return 0;
    int len = read<int>(arr, off::il2::Array_length);
    if (len <= 0 || len > 128) return 0;
    void** data = (void**)((char*)arr + off::il2::Array_data);
    if (!readable(data, sizeof(void*) * (size_t)len)) return 0;

    int n = 0;
    for (int i = 0; i < len && n < cap; i++) {
        void* hbc = data[i];
        if (!readable(hbc, 0x38)) continue;
        int bone = read<int>(hbc, off::fld::hbc_bone, -1);
        if (!interesting_bone(bone)) continue;
        void* tf = component_transform(hbc);
        if (!tf) continue;
        Vec3 p = transform_position(tf);
        if (p.bad()) continue;
        out[n].bone = bone;
        out[n].world = p;
        out[n].on_screen = false;
        n++;
    }
    return n;
}

bool los_clear(const Vec3& from, const Vec3& to) {
    RaycastHit hit{};
    float dist = (to - from).len();
    if (dist < 0.01f) return true;
    if (!linecast(from, to, &hit, ~0)) return true;      // между нами пусто
    return hit.distance >= dist - 0.75f;                  // упёрлись уже в саму цель
}

void fill_screen(PlayerView& p, void* camera, int sh) {
    // бокс по экранным границам костей, без костей — по росту игрока
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    int shown = 0;
    for (int i = 0; i < p.bone_count; i++) {
        Vec3 s = world_to_screen(camera, p.bones[i].world);
        if (s.z <= 0.05f) continue;
        float sx = s.x, sy = (float)sh - s.y;
        p.bones[i].screen = {sx, sy};
        p.bones[i].on_screen = true;
        minx = sx < minx ? sx : minx;
        maxx = sx > maxx ? sx : maxx;
        miny = sy < miny ? sy : miny;
        maxy = sy > maxy ? sy : maxy;
        shown++;
    }

    if (shown >= 2) {
        float pad = (maxx - minx) * 0.15f + 2.f;
        p.feet = {(minx + maxx) * 0.5f, maxy + pad * 0.3f};
        p.top = {(minx + maxx) * 0.5f, miny - pad * 0.6f};
        p.box_w = (maxx - minx) + pad * 2.f;
        p.on_screen = true;
        return;
    }

    Vec3 f = world_to_screen(camera, p.origin);
    Vec3 t = world_to_screen(camera, p.origin + Vec3{0.f, 1.8f, 0.f});
    if (f.z <= 0.05f || t.z <= 0.05f) {
        p.on_screen = false;
        return;
    }
    p.feet = {f.x, (float)sh - f.y};
    p.top = {t.x, (float)sh - t.y};
    p.box_w = (p.feet.y - p.top.y) * 0.45f;
    p.on_screen = true;
}

void consider_target(const PlayerView& p, const Frame& f, AimTarget& best) {
    if (g_cfg.aim_team_check && !p.enemy) return;
    if (p.dist > g_cfg.aim_max_dist) return;
    bool ok = false;
    Vec3 point = aim_point(p.ent, &ok);
    if (!ok) return;
    float a = angle_between(f.cam_fwd, point - f.cam_pos);
    if (a > g_cfg.aim_fov || a >= best.angle) return;
    if (g_cfg.aim_visible_check && !los_clear(f.cam_pos, point)) return;
    best.ok = true;
    best.ent = p.ent;
    best.point = point;
    best.angle = a;
}

bool build_player(void* e, void* camera, Frame& f, AimTarget& best) {
    if (f.count >= kMaxPlayers) return false;
    if (!is_class(e, off::cls::player)) return false;

    PlayerView p;
    p.ent = e;
    p.team = read<uint8_t>(e, off::fld::ake_team);
    p.enemy = p.team != f.local_team;

    void* stats = ptr(e, off::fld::ake_stats);
    if (readable(stats, 0x24)) {
        p.hp = read<uint16_t>(stats, off::fld::ylw_hp);
        p.maxhp = read<uint16_t>(stats, off::fld::ylw_maxhp, 100);
        p.armor = read<uint16_t>(stats, off::fld::ylw_armor);
    }
    if (p.hp <= 0 || p.hp > 1000) return false;
    if (p.maxhp <= 0) p.maxhp = 100;

    p.origin = player_position(e);
    if (p.origin.bad()) return false;
    p.dist = (p.origin - f.cam_pos).len();
    if (p.dist > g_cfg.esp_max_dist && p.dist > g_cfg.aim_max_dist) return false;

    p.bone_count = collect_bones(find_model(e), p.bones, kMaxBones);
    p.head = p.origin + Vec3{0.f, 1.55f, 0.f};
    for (int b = 0; b < p.bone_count; b++) {
        if (p.bones[b].bone == (int)off::Bone::Head) p.head = p.bones[b].world;
    }
    p.visible = los_clear(f.cam_pos, p.head);
    fill_screen(p, camera, f.screen_h);

    consider_target(p, f, best);
    f.players[f.count++] = p;
    return true;
}

void apply_visible_aim(void* camera_part, void* cam_tf, const Frame& f, const AimTarget& t) {
    void* rot = ptr(camera_part, off::fld::ios_rot);
    if (!readable(rot, 0x30)) return;
    Vec3 want = dir_to_euler(t.point - f.cam_pos);
    Vec3 cur = transform_euler(cam_tf);
    float smooth = g_cfg.aim_smooth < 1.f ? 1.f : g_cfg.aim_smooth;
    Vec3 step{
        cur.x + norm_angle(want.x - cur.x) / smooth,
        cur.y + norm_angle(want.y - cur.y) / smooth,
        0.f,
    };
    Quat q = euler_to_quat(step);
    if (g_cfg.aim_quat & 1) std::memcpy((char*)rot + off::fld::eir_quat_a, &q, sizeof(q));
    if (g_cfg.aim_quat & 2) std::memcpy((char*)rot + off::fld::eir_quat_b, &q, sizeof(q));
}

} // namespace

void note_player(void* ent) {
    if (!ent) return;
    int64_t ts = now_ms();
    std::lock_guard<std::mutex> lk(g_seen_mu);
    int oldest = 0;
    for (int i = 0; i < kSeenCap; i++) {
        if (g_seen[i].ent == ent) {
            g_seen[i].ts = ts;
            return;
        }
        if (g_seen[i].ts < g_seen[oldest].ts) oldest = i;
    }
    g_seen[oldest] = {ent, ts};
}

Vec3 aim_point(void* ent, bool* ok) {
    if (ok) *ok = false;
    if (!is_class(ent, off::cls::player)) return {};
    BoneView bones[kMaxBones];
    int n = collect_bones(find_model(ent), bones, kMaxBones);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            if (bones[i].bone == g_cfg.aim_bone) {
                if (ok) *ok = true;
                return bones[i].world;
            }
        }
        if (g_cfg.aim_fallback_body) {
            Vec3 sum{};
            for (int i = 0; i < n; i++) sum = sum + bones[i].world;
            if (ok) *ok = true;
            return sum * (1.f / (float)n);
        }
    }
    Vec3 origin = player_position(ent);
    if (origin.bad()) return {};
    if (ok) *ok = true;
    return origin + Vec3{0.f, 1.55f, 0.f};
}

AimTarget current_target() { return g_live.target; }

void copy_frame(Frame* out) {
    if (!out) return;
    std::lock_guard<std::mutex> lk(g_mu);
    *out = g_frame;
}

void tick(void* camera_part) {
    if (!readable(camera_part, 0x60)) return;

    void* local = ptr(camera_part, off::fld::chx_owner);
    void* world = ptr(camera_part, off::fld::chx_world);
    if (!is_class(local, off::cls::player)) return;

    void* camera = camera_main();
    if (!readable(camera, 0x40)) return;
    void* cam_tf = component_transform(camera);
    if (!cam_tf) return;

    g_live.camera = camera;
    g_live.pos = transform_position(cam_tf);
    g_live.fwd = transform_forward(cam_tf);
    g_live.w = camera_pixel_width(camera);
    g_live.h = camera_pixel_height(camera);
    g_live.fov = camera_fov(camera);
    g_live.local = local;
    g_live.team = read<uint8_t>(local, off::fld::ake_team);
    if (g_live.pos.bad() || g_live.w <= 0 || g_live.h <= 0) return;

    Frame f;
    f.ts = now_ms();
    f.screen_w = g_live.w;
    f.screen_h = g_live.h;
    f.fov = g_live.fov;
    f.cam_pos = g_live.pos;
    f.cam_fwd = g_live.fwd;
    f.local_team = g_live.team;

    AimTarget best;
    best.angle = 1e9f;

    // основной путь: реестр мира njh -> gat -> gzx -> yjk<krm>
    void* arr = nullptr;
    int count = 0;
    if (readable(world, 0x50)) {
        void* registry = ptr(world, off::fld::njh_registry);
        void* store = ptr(registry, off::fld::gat_store);
        void* entities = ptr(store, off::fld::gzx_entities);
        arr = ptr(entities, off::fld::yjk_array);
        count = read<int>(entities, off::fld::yjk_count);
    }
    if (readable(arr, off::il2::Array_data) && count > 0 && count <= 4096) {
        int cap = read<int>(arr, off::il2::Array_length);
        if (cap > 0 && count > cap) count = cap;
        f.entities_seen = count;
        void** data = (void**)((char*)arr + off::il2::Array_data);
        if (readable(data, sizeof(void*) * (size_t)count)) {
            for (int i = 0; i < count; i++) {
                if (data[i] == local) continue;
                build_player(data[i], camera, f, best);
            }
        }
    }

    // страховка: если реестр ничего не дал — берём то, что принёс хук ake.opmn
    if (f.count == 0) {
        Seen snap[kSeenCap];
        {
            std::lock_guard<std::mutex> lk(g_seen_mu);
            std::memcpy(snap, g_seen, sizeof(snap));
        }
        for (int i = 0; i < kSeenCap; i++) {
            if (!snap[i].ent || snap[i].ent == local) continue;
            if (f.ts - snap[i].ts > 500) continue;
            build_player(snap[i].ent, camera, f, best);
        }
    }

    g_live.target = best;
    if (best.ok) {
        Vec3 s = world_to_screen(camera, best.point);
        if (s.z > 0.05f) {
            f.has_target = true;
            f.target_screen = {s.x, (float)f.screen_h - s.y};
        }
        if (g_cfg.aim_visible) apply_visible_aim(camera_part, cam_tf, f, best);
    }

    std::lock_guard<std::mutex> lk(g_mu);
    g_frame = f;
}

} // namespace game
