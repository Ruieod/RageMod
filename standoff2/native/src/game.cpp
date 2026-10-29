// Character tracking, ESP projection, target selection and the glue into the aim controller.
// Everything here talks to the game only through readable Unity-layer types (CharacterBindings, BipedMap,
// TeamSpawnZone) and plain UnityEngine APIs, so it does not depend on the obfuscated ECS core.
#include "game.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <unordered_map>

#include "aim.h"
#include "bridge.h"
#include "model.h"
#include "unity.h"

namespace game {

World W;

float E(int slot) {
    if (slot >= 0 && slot < cfg::kN && cfg::kWIdx[slot] >= 0 && W.curWeapon >= 0 && W.curWeapon < cfg::kWeapons) {
        const int base = cfg::kWBase + W.curWeapon * cfg::kWStride;
        if (W.cfg[base] > 0.5f) return W.cfg[base + 1 + cfg::kWIdx[slot]];
    }
    return W.cfg[slot];
}

Color colorAt(int slot) { return {W.cfg[slot], W.cfg[slot + 1], W.cfg[slot + 2], W.cfg[slot + 3]}; }

namespace {

using il2::Ref;

// HumanBodyBones for: head, neck, chest, hips, L upper arm, L forearm, L hand, R upper arm, R forearm, R hand,
//                     L thigh, L calf, L foot, R thigh, R calf, R foot
constexpr int kBoneId[frame::kJoints] = {10, 9, 8, 0, 13, 15, 17, 14, 16, 18, 1, 3, 5, 2, 4, 6};
constexpr const char* kBipField[frame::kJoints] = {"Head", "Neck", "Spine2", "Hip", "LeftUpperarm", "LeftForearm", "LeftHand",
                                                    "RightUpperarm", "RightForearm", "RightHand", "LeftUpLeg", "LeftLeg",
                                                    "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot"};
enum Joint { J_HEAD, J_NECK, J_CHEST, J_HIPS, J_LFOOT = 12, J_RFOOT = 15 };

il2::Klass *K_CharBind = nullptr, *K_Biped = nullptr, *K_Zone = nullptr;
int O_anim = -1, O_rend = -1, O_tool = -1, O_bones = -1, O_zoneTeam = -1;
int O_bip[frame::kJoints];

struct Zone { Vec3 pos; int team; };
std::vector<Zone> g_zones;

aim::Controller g_aim;
Vec3 g_prevRefWorld;
bool g_haveRef = false;
Vec3 g_savedFwdWorld;
bool g_haveSaved = false;
bool g_prevEngaged = false;

double g_lastScan = 0, g_lastZoneScan = 0, g_lastStatus = 0, g_lastFireT = -10;
int g_killSeq = 0;
float g_killX = 0, g_killY = 0, g_killHs = 0;
int g_tickHooks = 0, g_exceptions = 0;
float g_tickMs = 0;
int g_lastTarget = -1;
bool g_fireHeld = false;
uint64_t g_ticks = 0;

// ---- silent camera lock (aim mode 3) ----
// The synthetic finger still turns the real view (that is what the game shoots along), but right before the frame is
// rendered the camera transform is put back to where the player was looking, and restored once the frame is drawn, so
// the flick never shows on screen. Pinned from Canvas.SendWillRenderCanvases / Camera.FireOnPreCull, restored from
// Camera.FireOnPostRender or, when the pipeline does not call that, at the start of the next tick.
struct Lock {
    bool active = false;
    float weight = 0;            // 1 = view fully held; fades out after the flick so the small return residual blends in
    Quat hold;                   // world rotation shown to the player
    double stamp = -10;          // last tick that refreshed the lock (a dead tick hook must never freeze the view)
    bool applied = false;        // the pinned rotation is in the camera right now
    int frame = -1;
    Ref tf;
    void* tfRaw = nullptr;
    Quat realLocal, pinLocal;
    int pins = 0, restores = 0, overwritten = 0, canvasHooks = 0, cullHooks = 0, postHooks = 0;
};
Lock g_lock;
Quat g_savedRot;

bool lockReady() {
    return unity::Transform_get_rotation.ok() && unity::Transform_set_rotation.ok() && unity::Transform_get_localRotation.ok() &&
           unity::Transform_set_localRotation.ok();
}

Quat lockView(Quat real) { return g_lock.weight >= 1.f ? g_lock.hold : nlerp(real, g_lock.hold, g_lock.weight); }

void restoreLock() {
    if (!g_lock.applied) return;
    g_lock.applied = false;
    try {
        void* tf = g_lock.tf.get();
        if (!tf || !unity::alive(tf)) return;
        if (qsame(unity::Transform_get_localRotation(tf), g_lock.pinLocal)) {
            unity::Transform_set_localRotation(tf, g_lock.realLocal);
            g_lock.restores++;
        } else {
            g_lock.overwritten++;   // the game already wrote a fresh rotation, nothing to undo
        }
    } catch (...) { g_exceptions++; }
}

// temporarily shows the held view to Camera.WorldToScreenPoint so the ESP matches what is rendered
struct ViewOverride {
    void* tf = nullptr;
    Quat local;
    ViewOverride(void* t) {
        if (!t || !g_lock.active || g_lock.weight <= 0.f || !lockReady()) return;
        local = unity::Transform_get_localRotation(t);
        unity::Transform_set_rotation(t, lockView(unity::Transform_get_rotation(t)));
        tf = t;
    }
    ~ViewOverride() {
        if (tf) { try { unity::Transform_set_localRotation(tf, local); } catch (...) { g_exceptions++; } }
    }
};

bool okVec(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

std::string lowerAlnum(const std::string& s) {
    std::string o;
    for (char ch : s) if (std::isalnum(static_cast<unsigned char>(ch))) o += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return o;
}

int detectWeapon(const std::string& raw) {
    const std::string n = lowerAlnum(raw);
    if (n.empty()) return -1;
    int best = -1;
    size_t bestLen = 0;
    for (int i = 0; i < cfg::kWeapons; i++) {
        for (const char* k : cfg::kWeaponDefs[i].keys) {
            if (!k) break;
            const std::string key = lowerAlnum(k);
            if (!key.empty() && key.size() > bestLen && n.find(key) != std::string::npos) { best = i; bestLen = key.size(); }
        }
    }
    return best;
}

void resolveBonesFromDictionary(Char& c, void* obj) {
    if (O_bones < 0) return;
    void* dict = il2::read<void*>(obj, O_bones);
    if (!dict) return;
    il2::Method* m = il2::methodIn(il2::classOf(dict), "TryGetValue", 2);
    if (!m) return;
    for (int j = 0; j < frame::kJoints; j++) {
        if (c.joint[j]) continue;
        int key = kBoneId[j];
        void* out = nullptr;
        void* args[2] = {&key, &out};
        bool threw = false;
        il2::invoke(m, dict, args, &threw);
        if (!threw && out) c.joint[j].set(out);
    }
}

std::unique_ptr<Char> makeChar(void* obj, bool biped, int id) {
    auto c = std::make_unique<Char>();
    c->id = id;
    c->biped = biped;
    c->bind.set(obj);
    void* tf = unity::Component_get_transform(obj);
    c->tf.set(tf);
    c->root.set(unity::Transform_get_root(tf));
    if (biped) {
        for (int j = 0; j < frame::kJoints; j++) {
            if (O_bip[j] < 0) continue;
            void* t = il2::read<void*>(obj, O_bip[j]);
            if (t) c->joint[j].set(t);
        }
    } else {
        void* anim = O_anim >= 0 ? il2::read<void*>(obj, O_anim) : nullptr;
        if (anim) {
            c->anim.set(anim);
            for (int j = 0; j < frame::kJoints; j++) {
                void* t = unity::Animator_GetBoneTransform(anim, kBoneId[j]);
                if (!t && j == J_CHEST) t = unity::Animator_GetBoneTransform(anim, 54);   // UpperChest
                if (!t && j == J_CHEST) t = unity::Animator_GetBoneTransform(anim, 7);    // Spine
                if (t) c->joint[j].set(t);
            }
        }
        if (!c->joint[J_HEAD]) resolveBonesFromDictionary(*c, obj);
        void* r = O_rend >= 0 ? il2::read<void*>(obj, O_rend) : nullptr;
        if (r) c->rend.set(r);
    }
    return c;
}

Char* findChar(int id) {
    for (auto& c : W.chars) if (c->id == id) return c.get();
    return nullptr;
}

void rescan() {
    std::vector<void*> a = unity::findAll(K_CharBind);
    std::vector<void*> b = K_Biped ? unity::findAll(K_Biped) : std::vector<void*>();
    std::vector<int> seen;
    auto take = [&](void* obj, bool biped) {
        try {
            const int id = unity::Object_GetInstanceID(obj);
            seen.push_back(id);
            Char* c = findChar(id);
            if (!c) {
                auto nc = makeChar(obj, biped, id);
                nc->firstSeen = W.now;
                nc->lastSeen = W.now;
                W.chars.push_back(std::move(nc));
            } else {
                c->lastSeen = W.now;
                // late-bound bones (animator not ready on the first scan)
                if (!c->biped && !c->joint[J_HEAD]) {
                    void* anim = O_anim >= 0 ? il2::read<void*>(obj, O_anim) : nullptr;
                    if (anim) {
                        for (int j = 0; j < frame::kJoints; j++) {
                            if (c->joint[j]) continue;
                            void* t = unity::Animator_GetBoneTransform(anim, kBoneId[j]);
                            if (t) c->joint[j].set(t);
                        }
                    }
                }
            }
        } catch (...) { g_exceptions++; }
    };
    for (void* o : a) take(o, false);
    for (void* o : b) take(o, true);

    // sweep: characters that vanished
    int removed = 0;
    for (auto& c : W.chars) if (std::find(seen.begin(), seen.end(), c->id) == seen.end()) removed++;
    for (size_t i = 0; i < W.chars.size();) {
        Char& c = *W.chars[i];
        if (std::find(seen.begin(), seen.end(), c.id) != seen.end()) { i++; continue; }
        const bool recentlyShot = W.now - g_lastFireT < 1.6;
        const bool wasOnScreen = W.now - c.lastSeenOnScreen < 0.8;
        if (!c.local && !c.teammate && removed <= 2 && recentlyShot && wasOnScreen && c.dist < 150.f) {
            g_killSeq++;
            g_killX = c.lastHeadScreen.x;
            g_killY = c.lastHeadScreen.y;
            g_killHs = 0.f;
            bridge::pushVibrate();
        }
        W.chars.erase(W.chars.begin() + static_cast<long>(i));
    }
}

void rescanZones() {
    g_zones.clear();
    if (!K_Zone || O_zoneTeam < 0) return;
    for (void* z : unity::findAll(K_Zone)) {
        try {
            Zone zz;
            zz.team = il2::read<uint8_t>(z, O_zoneTeam);
            zz.pos = unity::Transform_get_position(unity::Component_get_transform(z));
            if (okVec(zz.pos)) g_zones.push_back(zz);
        } catch (...) {}
    }
}

bool updateChar(Char& c) {
    try {
        void* tf = c.tf.get();
        if (!tf || !unity::alive(tf)) return false;
        c.prevPos = c.pos;
        c.pos = unity::Transform_get_position(tf);
        int nv = 0;
        for (int j = 0; j < frame::kJoints; j++) {
            void* t = c.joint[j].get();
            c.jwv[j] = false;
            if (!t) continue;
            const Vec3 p = unity::Transform_get_position(t);
            if (!okVec(p) || (p.x == 0 && p.y == 0 && p.z == 0)) continue;
            c.jw[j] = p;
            c.jwv[j] = true;
            nv++;
        }
        if (!okVec(c.pos)) return false;
        c.head = c.jwv[J_HEAD] ? c.jw[J_HEAD] + Vec3{0, 0.07f, 0} : c.pos + Vec3{0, 1.55f, 0};
        c.foot = c.pos;
        float lowY = 1e9f;
        bool any = false;
        for (int j : {int(J_LFOOT), int(J_RFOOT)}) if (c.jwv[j]) { lowY = std::min(lowY, c.jw[j].y); any = true; }
        if (any) c.foot = Vec3{c.pos.x, lowY, c.pos.z};
        c.valid = nv >= 3 || c.jwv[J_HEAD];
        c.dist = len(c.head - W.camPos);
        return true;
    } catch (...) { g_exceptions++; return false; }
}

void refreshTools() {
    for (auto& up : W.chars) {
        Char& c = *up;
        if (c.biped || W.now - c.toolT < 1.0) continue;
        c.toolT = W.now;
        try {
            void* obj = c.bind.get();
            void* go = (obj && O_tool >= 0) ? il2::read<void*>(obj, O_tool) : nullptr;
            std::string n = go && unity::alive(go) ? unity::nameOf(go) : std::string();
            if (n != c.toolName) {
                c.toolName = n;
                c.weapon = detectWeapon(n);
                bridge::setToolName(c.id, n);
            }
        } catch (...) {}
    }
}

void updateTeams() {
    for (auto& up : W.chars) {
        Char& c = *up;
        if (len(c.pos - c.prevPos) > 10.f) c.team = 0;   // respawn / teleport: team must be re-derived
        if (c.team == 0 && !g_zones.empty()) {
            const float speed = len(c.pos - c.prevPos) / std::max(W.dt, 0.001f);
            if (speed < 2.5f) {
                float bd = 1e9f;
                int bt = 0;
                for (const Zone& z : g_zones) {
                    const float d = len(z.pos - c.pos);
                    if (d < bd) { bd = d; bt = z.team; }
                }
                if (bd < 22.f && (bt == 1 || bt == 2)) c.team = bt;
            }
        }
    }
    static double lastLocalTeamT = 0;
    if (W.local && W.local->team) { W.localTeam = W.local->team; lastLocalTeamT = W.now; }
    else if (W.now - lastLocalTeamT > 15.0) W.localTeam = 0;
    for (auto& up : W.chars) {
        Char& c = *up;
        c.teammate = !c.local && c.team != 0 && W.localTeam != 0 && c.team == W.localTeam;
    }
}

Vec2 toView(Vec3 p) { return {p.x, static_cast<float>(W.camH) - p.y}; }   // Unity (y-up) -> y-down camera pixels

bool project(Vec3 w, Vec2& out, float* z = nullptr) {
    bool front = false;
    const Vec3 p = unity::w2s(W.cam, w, &front);
    out = toView(p);
    if (z) *z = p.z;
    return front && std::isfinite(p.x) && std::isfinite(p.y);
}

bool lineClear(Char& c, Vec3 target) {
    const Vec3 d = target - W.camPos;
    const float dist = len(d);
    if (dist < 0.6f) return true;
    const Vec3 dir = d * (1.f / dist);
    const Vec3 origin = W.camPos + dir * 0.45f;   // skip our own capsule
    alignas(16) unsigned char hit[96] = {};
    bool any = false;
    try { any = unity::Physics_Raycast(origin, dir, hit, dist - 0.45f - 0.05f, -1, 1 /* QueryTriggerInteraction.Ignore */); }
    catch (...) { return true; }
    if (!any) return true;
    try {
        void* col = unity::RaycastHit_get_collider(hit);
        if (!col) return false;
        void* t = unity::Component_get_transform(col);
        void* root = unity::Transform_get_root(t);
        return root == c.root.get() || t == c.tf.get();
    } catch (...) { return false; }
}

void updateVisibility(Char& c) {
    if (c.local || !c.valid) return;
    if (W.now - c.lastVisCheck < 0.045) return;
    c.lastVisCheck = W.now;
    const Vec3 headP = c.head;
    const Vec3 chestP = c.jwv[J_CHEST] ? c.jw[J_CHEST] : lerp(c.head, c.foot, 0.3f);
    c.headVis = lineClear(c, headP);
    c.chestVis = c.headVis ? true : lineClear(c, chestP);
    c.visible = c.headVis || c.chestVis;
}

struct Pt { Vec3 w; bool vis; };
Pt aimPoint(Char& c, int hb, Vec3 ghostHead, bool useGhost) {
    const Vec3 head = useGhost ? ghostHead : c.head;
    const Vec3 neck = c.jwv[J_NECK] ? c.jw[J_NECK] : head - Vec3{0, 0.15f, 0};
    const Vec3 chest = c.jwv[J_CHEST] ? c.jw[J_CHEST] : lerp(c.head, c.foot, 0.3f);
    const Vec3 hips = c.jwv[J_HIPS] ? c.jw[J_HIPS] : lerp(c.head, c.foot, 0.5f);
    const Vec3 shift = useGhost ? (ghostHead - c.head) : Vec3{0, 0, 0};
    switch (hb) {
        case 0: return {head, c.headVis};
        case 1: return {neck + shift, c.headVis || c.chestVis};
        case 2: return {chest + shift, c.chestVis};
        case 3: return {hips + shift, c.chestVis};
        case 5: return c.headVis ? Pt{head, true} : Pt{chest + shift, c.chestVis};
        default: return {head, c.headVis};
    }
}

bool ghostAt(Char& c, double age, Vec3* head, Vec3* foot) {
    if (c.hist.size() < 2) return false;
    const double want = W.now - age;
    const Hist* pick = &c.hist.front();
    for (const Hist& h : c.hist) { if (h.t <= want) pick = &h; else break; }
    if (W.now - pick->t < age * 0.5) return false;
    *head = pick->head;
    *foot = pick->foot;
    return true;
}

float hitRadiusM(int hb) { return hb == 0 ? 0.13f : (hb == 1 ? 0.12f : 0.22f); }

void fillPlayer(float* f, Char& c, bool isTarget, int hbForGhost) {
    (void)hbForGhost;
    f[frame::P_ID] = static_cast<float>(c.id);
    int flags = 0;
    if (!c.teammate) flags |= frame::PF_ENEMY;
    if (c.visible) flags |= frame::PF_VISIBLE;
    if (isTarget) flags |= frame::PF_TARGET;
    if (c.team) flags |= frame::PF_TEAMKNOWN;
    f[frame::P_DIST] = c.dist;
    f[frame::P_WEAPON] = static_cast<float>(c.weapon);
    f[frame::P_TEAM] = static_cast<float>(c.team);
    f[frame::P_HP] = -1.f;

    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    int n = 0;
    for (int j = 0; j < frame::kJoints; j++) {
        float* jj = f + frame::P_J + j * 2;
        jj[0] = jj[1] = NAN;
        if (!c.jwv[j]) continue;
        Vec2 s;
        if (!project(c.jw[j], s)) continue;
        jj[0] = s.x; jj[1] = s.y;
        minx = std::min(minx, s.x); maxx = std::max(maxx, s.x);
        miny = std::min(miny, s.y); maxy = std::max(maxy, s.y);
        n++;
    }
    Vec2 head, foot;
    const bool hOk = project(c.head, head);
    const bool fOk = project(c.foot, foot);
    if (hOk) { minx = std::min(minx, head.x); maxx = std::max(maxx, head.x); miny = std::min(miny, head.y - 0.0f); maxy = std::max(maxy, head.y); n++; }
    if (fOk) { minx = std::min(minx, foot.x); maxx = std::max(maxx, foot.x); miny = std::min(miny, foot.y); maxy = std::max(maxy, foot.y); n++; }
    f[frame::P_HX] = hOk ? head.x : NAN;
    f[frame::P_HY] = hOk ? head.y : NAN;
    if (n >= 2) {
        float h = maxy - miny;
        float w = maxx - minx;
        if (hOk && fOk) {   // enforce a human aspect ratio when limbs are not spread
            const float wantW = std::fabs(foot.y - head.y) * 0.42f;
            if (w < wantW) { const float cx = (minx + maxx) * 0.5f; minx = cx - wantW * 0.5f; maxx = cx + wantW * 0.5f; w = wantW; }
        }
        const float pad = std::max(2.f, h * 0.07f);
        f[frame::P_BX1] = minx - pad; f[frame::P_BY1] = miny - pad * 1.4f;
        f[frame::P_BX2] = maxx + pad; f[frame::P_BY2] = maxy + pad * 0.6f;
        flags |= frame::PF_BOX;
        c.onScreen = true;
        c.sHead = hOk ? head : Vec2{(minx + maxx) * 0.5f, miny};
        c.lastHeadScreen = c.sHead;
        c.lastSeenOnScreen = W.now;
    } else {
        c.onScreen = false;
    }
    // off-screen direction
    if (!(flags & frame::PF_BOX) || f[frame::P_BX2] < 0 || f[frame::P_BX1] > W.camW || f[frame::P_BY2] < 0 || f[frame::P_BY1] > W.camH) {
        Vec2 s;
        float z = 0;
        const bool front = project(c.pos + Vec3{0, 1.0f, 0}, s, &z);
        float dx = s.x - W.camW * 0.5f, dy = s.y - W.camH * 0.5f;
        if (!front) { dx = -dx; dy = -dy; }
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l > 1.f) { f[frame::P_AX] = dx / l; f[frame::P_AY] = dy / l; flags |= frame::PF_OFFSCREEN; }
    }
    // backtrack ghost
    if (GB(cfg::C_BT_ENABLE) && GB(cfg::C_BT_GHOST)) {
        Vec3 gh, gf;
        if (ghostAt(c, G(cfg::C_BT_MS) / 1000.0, &gh, &gf)) {
            Vec2 a, b;
            if (project(gh, a) && project(gf, b)) {
                const float hh = std::fabs(b.y - a.y);
                f[frame::P_GX1] = a.x - hh * 0.22f; f[frame::P_GX2] = a.x + hh * 0.22f;
                f[frame::P_GY1] = std::min(a.y, b.y) - hh * 0.07f; f[frame::P_GY2] = std::max(a.y, b.y);
                flags |= frame::PF_GHOST;
            }
        }
    }
    f[frame::P_FLAGS] = static_cast<float>(flags);
}

void pushHistory(Char& c) {
    c.hist.push_back({W.now, c.head, c.foot});
    while (!c.hist.empty() && W.now - c.hist.front().t > 0.6) c.hist.pop_front();
}

}  // namespace

bool init() {
    K_CharBind = il2::klass("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings");
    K_Biped = il2::klass("Axlebolt.Standoff.Player", "BipedMap");
    K_Zone = il2::klass("Axlebolt.Standoff.Level.Definitions.Zone.Spawn", "TeamSpawnZone");
    O_anim = il2::fieldOffset(il2::field("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings", "<Animator>k__BackingField"));
    O_rend = il2::fieldOffset(il2::field("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings", "<CharacterRenderer>k__BackingField"));
    O_tool = il2::fieldOffset(il2::field("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings", "<Tool>k__BackingField"));
    O_bones = il2::fieldOffset(il2::field("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings", "_bones"));
    O_zoneTeam = il2::fieldOffset(il2::field("Axlebolt.Standoff.Level.Definitions.Zone.Spawn", "TeamSpawnZone", "_team"));
    for (int j = 0; j < frame::kJoints; j++) O_bip[j] = il2::fieldOffset(il2::field("Axlebolt.Standoff.Player", "BipedMap", kBipField[j]));
    rlog("game: CharacterBindings=%d BipedMap=%d TeamSpawnZone=%d anim@%d rend@%d tool@%d zoneTeam@%d", K_CharBind != nullptr, K_Biped != nullptr,
         K_Zone != nullptr, O_anim, O_rend, O_tool, O_zoneTeam);
    visualsInit();
    return K_CharBind != nullptr || K_Biped != nullptr;
}

void tick() {
    const double t0 = nowSec();
    g_ticks++;
    restoreLock();   // the game and the measurements below must see the real view
    int fc = 0;
    try { fc = unity::Time_get_frameCount(); } catch (...) { return; }
    if (fc == W.frameNo) return;   // several hooks may call us in the same frame
    W.dt = static_cast<float>(std::min(0.1, std::max(0.001, t0 - W.now)));
    W.frameNo = fc;
    W.now = t0;
    g_tickHooks++;
    bridge::snapshotConfig(W.cfg, cfg::kTotal);

    float fbuf[frame::kTotal];
    std::memset(fbuf, 0, sizeof fbuf);
    const bridge::Input in = bridge::getInput();
    float* out = fbuf;

    try {
        W.cam = unity::Camera_get_main();
        if (!W.cam || !unity::alive(W.cam)) {
            W.cam = nullptr;
            out[frame::H_COUNT] = 0;
            out[frame::H_FLAGS] = frame::HF_TICK;
            out[frame::H_TARGET] = -1;
            out[frame::H_KILLSEQ] = static_cast<float>(g_killSeq);
            bridge::publishFrame(out, frame::kTotal);
            return;
        }
        W.camW = std::max(1, unity::Camera_get_pixelWidth(W.cam));
        W.camH = std::max(1, unity::Camera_get_pixelHeight(W.cam));
        void* ctf = unity::Component_get_transform(W.cam);
        W.camPos = unity::Transform_get_position(ctf);
        W.camFwd = unity::Transform_get_forward(ctf);
        const Quat camRot = lockReady() ? unity::Transform_get_rotation(ctf) : Quat{};
        W.vfov = clampf(unity::Camera_get_fieldOfView(W.cam), 20.f, 120.f);

        if (W.now - g_lastScan > 0.25) { g_lastScan = W.now; rescan(); }
        if (W.now - g_lastZoneScan > 3.0) { g_lastZoneScan = W.now; rescanZones(); }

        // positions
        W.local = nullptr;
        float bestLocal = 1.15f;
        for (auto& up : W.chars) {
            Char& c = *up;
            c.local = false;
            if (!updateChar(c)) { c.valid = false; continue; }
            if (c.dist < bestLocal) { bestLocal = c.dist; W.local = &c; }
        }
        // anything within arm's reach of the camera is us (body + arms rigs)
        for (auto& up : W.chars) if (up->valid && up->dist < 1.15f) up->local = true;
        if (W.local) W.local->local = true;
        refreshTools();
        updateTeams();

        // active weapon profile
        const int manual = static_cast<int>(G(cfg::C_WPN_MANUAL));
        int cw = -1;
        if (manual > 0 && manual <= cfg::kWeapons) cw = manual - 1;
        else if (W.local) cw = W.local->weapon;
        W.curWeapon = cw;

        for (auto& up : W.chars) {
            Char& c = *up;
            if (!c.valid || c.local) continue;
            pushHistory(c);
            updateVisibility(c);
        }

        // ---- ESP frame ----
        const float cxU = W.camW * 0.5f, cyU = W.camH * 0.5f;
        const float cross_x = cxU, cross_y = cyU + G(cfg::C_CAL_CROSS);   // y-down camera pixels
        const float maxEsp = G(cfg::C_ESP_MAXDIST);
        const float maxAim = G(cfg::C_AIM_MAXDIST);
        const float focalPx = (W.camH * 0.5f) / std::tan(W.vfov * 0.5f * 3.14159265f / 180.f);
        const float fovDeg = clampf(E(cfg::C_AIM_FOV), 1.f, 80.f);
        const float fovPx = std::tan(fovDeg * 3.14159265f / 180.f) * focalPx;

        std::vector<Char*> list;
        for (auto& up : W.chars) {
            Char& c = *up;
            if (!c.valid || c.local) continue;
            if (c.dist > std::max(maxEsp, maxAim) + 5.f) continue;
            list.push_back(&c);
        }
        std::sort(list.begin(), list.end(), [](Char* a, Char* b) { return a->dist < b->dist; });
        if (list.size() > static_cast<size_t>(frame::kMaxPlayers)) list.resize(frame::kMaxPlayers);

        // ---- targeting ----
        const bool aimOn = EB(cfg::C_AIM_ENABLE);
        const int hb = static_cast<int>(E(cfg::C_AIM_HITBOX));
        const bool useGhost = GB(cfg::C_BT_ENABLE) && GB(cfg::C_BT_AIM);
        Char* target = nullptr;
        Vec2 targetErr{0, 0};
        float targetHitPx = 12.f;
        float bestScore = 1e9f;
        bool afAligned = false;
        const bool afOn = EB(cfg::C_AF_ENABLE);
        const float afRadius = E(cfg::C_AF_RADIUS);

        for (Char* c : list) {
            // --- aimbot candidate ---
            if (aimOn && c->dist <= maxAim && !(c->teammate && GB(cfg::C_AIM_TEAM))) {
                Vec3 gh, gf;
                const bool haveGhost = useGhost && ghostAt(*c, G(cfg::C_BT_MS) / 1000.0, &gh, &gf);
                std::vector<int> hbs;
                if (hb == 4) hbs = {0, 1, 2, 3}; else hbs = {hb};
                for (int h : hbs) {
                    Pt p = aimPoint(*c, h, haveGhost ? gh : c->head, haveGhost);
                    if (GB(cfg::C_AIM_VISIBLE) && !p.vis) continue;
                    Vec2 s;
                    if (!project(p.w, s)) continue;
                    const Vec2 err{s.x - cross_x, s.y - cross_y};
                    const float e = std::sqrt(err.x * err.x + err.y * err.y);
                    if (e > fovPx) continue;
                    const float score = G(cfg::C_AIM_PRIO) > 0.5f ? c->dist : e;
                    if (score < bestScore) {
                        bestScore = score; target = c; targetErr = err;
                        targetHitPx = std::max(3.f, hitRadiusM(h) * focalPx / std::max(c->dist, 1.f));
                    }
                }
            }
        }
        // --- autofire alignment: any allowed enemy hitbox under the crosshair ---
        if (afOn) {
            for (Char* c : list) {
                if (c->teammate && GB(cfg::C_AF_TEAM)) continue;
                if (c->dist > std::max(maxAim, 40.f)) continue;
                if (GB(cfg::C_AF_VISIBLE) && !c->visible) continue;
                const int pts[4] = {J_HEAD, J_NECK, J_CHEST, J_HIPS};
                for (int k = 0; k < 4; k++) {
                    Vec3 w = k == 0 ? c->head : (c->jwv[pts[k]] ? c->jw[pts[k]] : Vec3{0, 0, 0});
                    if (k > 0 && !c->jwv[pts[k]]) continue;
                    Vec2 s;
                    if (!project(w, s)) continue;
                    const float e = std::sqrt((s.x - cross_x) * (s.x - cross_x) + (s.y - cross_y) * (s.y - cross_y));
                    const float body = std::max(3.f, hitRadiusM(k == 0 ? 0 : (k == 1 ? 1 : 2)) * focalPx / std::max(c->dist, 1.f));
                    if (e <= std::min(afRadius, body * 1.15f)) { afAligned = true; break; }
                }
                if (afAligned) break;
            }
        }

        // ---- camera shift (for gain estimation + RCS) ----
        Vec2 camShift{0, 0};
        if (g_haveRef) {
            bool front = false;
            const Vec3 p = unity::w2s(W.cam, g_prevRefWorld, &front);
            if (front) camShift = {p.x - cxU, -(p.y - cyU)};
        }
        g_prevRefWorld = W.camPos + W.camFwd * 100.f;
        g_haveRef = true;
        if (!g_prevEngaged) { g_savedFwdWorld = W.camPos + W.camFwd * 100.f; g_savedRot = camRot; g_haveSaved = true; }

        // ---- controller ----
        aim::In ai;
        ai.dt = W.dt;
        ai.haveTarget = target != nullptr;
        ai.err = targetErr;
        ai.inFov = target != nullptr;
        ai.aimEnabled = aimOn && in.menuOpen < 0.5f;
        ai.aimAllowed = G(cfg::C_AIM_TRIGGER) < 0.5f || in.userFiring > 0.5f || g_fireHeld;
        ai.mode = static_cast<int>(E(cfg::C_AIM_MODE));
        ai.smooth = clampf(E(cfg::C_AIM_SMOOTH) / 100.f, 0.f, 1.f);
        ai.afEnabled = afOn && in.menuOpen < 0.5f;
        ai.afAligned = afAligned;
        ai.afMode = static_cast<int>(E(cfg::C_AF_MODE));
        ai.afDelayMs = E(cfg::C_AF_DELAY);
        ai.afTapMs = E(cfg::C_AF_TAP);
        ai.afIntervalMs = E(cfg::C_AF_INTERVAL);
        ai.tolPx = std::min(afRadius, targetHitPx);
        ai.userFiring = in.userFiring > 0.5f;
        ai.userLooking = in.userLooking > 0.5f;
        ai.camShift = camShift;
        ai.rcsEnabled = EB(cfg::C_RCS_ENABLE) && in.menuOpen < 0.5f;
        ai.rcsX = E(cfg::C_RCS_X) / 100.f;
        ai.rcsY = E(cfg::C_RCS_Y) / 100.f;
        ai.rcsGainPx = E(cfg::C_RCS_GAIN);
        ai.sens = G(cfg::C_CAL_SENS);
        ai.autoTune = GB(cfg::C_CAL_AUTO);
        ai.maxStepPx = 0.45f * static_cast<float>(std::min(W.camW, W.camH));
        if (g_haveSaved) {
            bool front = false;
            const Vec3 p = unity::w2s(W.cam, g_savedFwdWorld, &front);
            ai.haveReturn = front;
            ai.returnErr = {p.x - cross_x, (static_cast<float>(W.camH) - p.y) - cross_y};
        }
        // the finger deltas are in *view* px; the controller already works in camera px, Java scales by view/camera ratio
        const aim::Out ao = g_aim.update(ai);
        g_prevEngaged = ao.engaged;
        g_fireHeld = ao.fireHeld;
        if (ao.fireHeld || in.userFiring > 0.5f) g_lastFireT = W.now;
        const bool allow = in.menuOpen < 0.5f && !GB(cfg::C_CAL_ON);
        const bool silent = ai.mode == 3 && ai.aimEnabled;
        bridge::pushCommand(allow && ao.lookActive, allow ? ao.finger.x : 0.f, allow ? ao.finger.y : 0.f, allow && ao.fireHeld,
                            allow && silent && (target != nullptr || ao.engaged));

        // camera lock: hold the view the flick started from for as long as the controller is engaged
        if (silent && ao.engaged && lockReady()) {
            if (!g_lock.active) g_lock.hold = g_savedRot;
            g_lock.active = true;
            g_lock.weight = 1.f;
        } else if (g_lock.active) {
            g_lock.weight = silent ? g_lock.weight - W.dt / 0.09f : 0.f;
            if (g_lock.weight <= 0.f) { g_lock.weight = 0.f; g_lock.active = false; }
        }
        g_lock.stamp = W.now;

        // ---- publish ----
        out[frame::H_W] = static_cast<float>(W.camW);
        out[frame::H_H] = static_cast<float>(W.camH);
        out[frame::H_FLAGS] = static_cast<float>(frame::HF_CAMERA | frame::HF_TICK | (W.local ? frame::HF_LOCAL : 0) | (ao.engaged ? frame::HF_ENGAGED : 0) |
                                                  (ao.fireHeld ? frame::HF_FIRE : 0));
        out[frame::H_FOVPX] = aimOn ? fovPx : 0.f;
        out[frame::H_CX] = cross_x;
        out[frame::H_CY] = cross_y;
        out[frame::H_TARGET] = -1;
        int idx = 0;
        {
            ViewOverride shown(ctf);
            for (Char* c : list) {
                float* pf = out + frame::kHeader + idx * frame::kStride;
                fillPlayer(pf, *c, c == target, hb);
                if (c == target) out[frame::H_TARGET] = static_cast<float>(idx);
                c->slot = idx;
                idx++;
            }
        }
        out[frame::H_COUNT] = static_cast<float>(idx);
        out[frame::H_KILLSEQ] = static_cast<float>(g_killSeq);
        out[frame::H_KILLX] = g_killX;
        out[frame::H_KILLY] = g_killY;
        out[frame::H_KILLHS] = g_killHs;
        out[frame::H_LOCALTEAM] = static_cast<float>(W.localTeam);
        out[frame::H_WEAPON] = static_cast<float>(W.curWeapon);
        out[frame::H_GAINX] = ao.gain.x;
        out[frame::H_GAINY] = ao.gain.y;
        out[frame::H_LAG] = static_cast<float>(g_aim.lag());
        out[frame::H_CHARS] = static_cast<float>(W.chars.size());
        out[frame::H_ZONES] = static_cast<float>(g_zones.size());
        g_lastTarget = target ? target->id : -1;

        visualsTick();
    } catch (...) {
        g_exceptions++;
    }
    g_tickMs = static_cast<float>((nowSec() - t0) * 1000.0);
    out[frame::H_TICKMS] = g_tickMs;
    bridge::publishFrame(out, frame::kTotal);

    if (W.now - g_lastStatus > 0.5) {
        g_lastStatus = W.now;
        bridge::setStatus(describe());
    }
}

std::string describe() {
    char b[640];
    snprintf(b, sizeof b,
             "tick hooks: %d (%.2f ms)  exceptions: %d\nresolved: %d ok / %d failed\ncharacters: %zu  zones: %zu  local: %s  team: %d\nweapon: %s%s\naim gain: %.2f / %.2f  lag: %d  samples: %d\nkills: %d  last target: %d\n%s",
             g_tickHooks, g_tickMs, g_exceptions, il2::stat().ok, il2::stat().fail, W.chars.size(), g_zones.size(),
             W.local ? "yes" : "no", W.localTeam,
             W.curWeapon >= 0 ? cfg::kWeaponDefs[W.curWeapon].name : "unknown",
             W.local && !W.local->toolName.empty() ? (std::string(" (tool '") + W.local->toolName + "')").c_str() : "",
             g_aim.gain().x, g_aim.gain().y, g_aim.lag(), g_aim.samples(), g_killSeq, g_lastTarget, visualsDescribe().c_str());
    std::string s = b;
    char l[200];
    snprintf(l, sizeof l, "\nsilent lock: %s  hooks canvas %d / cull %d / post %d  pins %d  restores %d  overwritten %d",
             !lockReady() ? "n/a" : (g_lock.active ? "holding" : "idle"), g_lock.canvasHooks, g_lock.cullHooks, g_lock.postHooks,
             g_lock.pins, g_lock.restores, g_lock.overwritten);
    s += l;
    if (il2::stat().fail > 0) { s += "\nmissing: "; s += il2::stat().failed; }
    return s;
}

void preRender(void* cam) {
    if (cam) g_lock.cullHooks++; else g_lock.canvasHooks++;
    if (!g_lock.applied && !g_lock.active) return;
    try {
        void* main = unity::Camera_get_main();
        if (!main || (cam && cam != main)) return;
        const int fc = unity::Time_get_frameCount();
        if (g_lock.applied && g_lock.frame == fc) return;   // canvas pass already pinned this frame
        restoreLock();
        if (!g_lock.active || g_lock.weight <= 0.f || nowSec() - g_lock.stamp > 0.25 || !lockReady()) return;
        void* tf = unity::Component_get_transform(main);
        if (tf != g_lock.tfRaw) { g_lock.tf.set(tf); g_lock.tfRaw = tf; }
        g_lock.realLocal = unity::Transform_get_localRotation(tf);
        unity::Transform_set_rotation(tf, lockView(unity::Transform_get_rotation(tf)));
        g_lock.pinLocal = unity::Transform_get_localRotation(tf);
        g_lock.applied = true;
        g_lock.frame = fc;
        g_lock.pins++;
    } catch (...) { g_exceptions++; }
}

void postRender(void* cam) {
    g_lock.postHooks++;
    if (!g_lock.applied || !cam) return;
    try {
        if (unity::Component_get_transform(cam) == g_lock.tfRaw) restoreLock();
    } catch (...) { g_exceptions++; }
}

}  // namespace game
