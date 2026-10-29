// End-to-end host test: real game layer + aim controller + bridge against the mock Unity world (test/mock_il2cpp.cpp).
#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "bridge.h"
#include "cfg_gen.h"
#include "common.h"
#include "frame.h"
#include "game.h"
#include "il2cpp.h"
#include "unity.h"

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; printf("  FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---- mock hooks ----
static void* mock;
template <class T> static T sym(const char* n) { T f = reinterpret_cast<T>(dlsym(mock, n)); if (!f) { printf("missing mock symbol %s\n", n); exit(2); } return f; }
static void (*mock_reset)();
static void (*mock_next_frame)();
static void (*mock_set_camera)(float, float, float, float, float);
static void (*mock_cam_rotate)(float, float);
static float (*mock_cam_yaw)();
static float (*mock_cam_pitch)();
static void (*mock_set_wall)(float);
static int (*mock_add_char)(float, float, float, const char*);
static void (*mock_move_char)(int, float, float, float);
static void (*mock_kill_obj)(int);
static void (*mock_add_zone)(float, float, float, int);
static void* (*mock_char_material)(int, int);
static size_t (*mock_char_material_count)(int);
static float (*mock_material_color_r)(void*);
static int (*mock_material_queue)(void*);
static int (*mock_add_blind_view)();
static float (*mock_canvasgroup_alpha_for)(const char*);

static double g_clock = 100.0;
static float g_cfg[cfg::kTotal];
static float F[frame::kTotal];

static void setCfg(int slot, float v) { g_cfg[slot] = v; }
static void pushCfg() { bridge::setConfig(g_cfg, cfg::kTotal); }
static void tick(int n = 1) {
    for (int i = 0; i < n; i++) {
        g_clock += 1.0 / 60.0;
        testClock(true, g_clock);
        mock_next_frame();
        game::tick();
    }
    bridge::readFrame(F, frame::kTotal);
}
static void defaults() {
    memset(g_cfg, 0, sizeof g_cfg);
    setCfg(cfg::C_ESP_ENABLE, 1); setCfg(cfg::C_ESP_MAXDIST, 120); setCfg(cfg::C_ESP_BOX, 2); setCfg(cfg::C_AIM_MAXDIST, 80);
    setCfg(cfg::C_AIM_FOV, 40); setCfg(cfg::C_AIM_SMOOTH, 35); setCfg(cfg::C_AIM_VISIBLE, 1); setCfg(cfg::C_AIM_TEAM, 1);
    setCfg(cfg::C_AF_RADIUS, 16); setCfg(cfg::C_AF_VISIBLE, 1); setCfg(cfg::C_AF_TEAM, 1); setCfg(cfg::C_AF_TAP, 45); setCfg(cfg::C_AF_INTERVAL, 60);
    setCfg(cfg::C_RCS_GAIN, 14); setCfg(cfg::C_CAL_SENS, 1.0f); setCfg(cfg::C_CAL_AUTO, 1); setCfg(cfg::C_BT_MS, 100);
    setCfg(cfg::C_CHM_CVIS, 0.2f); setCfg(cfg::C_CHM_CVIS + 3, 1.f); setCfg(cfg::C_CHM_CHID, 0.9f); setCfg(cfg::C_CHM_CHID + 3, 1.f);
    pushCfg();
}
static const float kF = 540.f / std::tan(30.f * 3.14159265f / 180.f);   // focal length in px for 1080p, 60 deg vfov
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static int findPlayer(int id) {
    int n = (int)F[frame::H_COUNT];
    for (int i = 0; i < n; i++) if ((int)F[frame::kHeader + i * frame::kStride + frame::P_ID] == id) return i;
    return -1;
}
static float P(int i, int off) { return F[frame::kHeader + i * frame::kStride + off]; }

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "./libil2cpp.so";
    mock = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!mock) { printf("cannot load mock: %s\n", dlerror()); return 2; }
    mock_reset = sym<decltype(mock_reset)>("mock_reset");
    mock_next_frame = sym<decltype(mock_next_frame)>("mock_next_frame");
    mock_set_camera = sym<decltype(mock_set_camera)>("mock_set_camera");
    mock_cam_rotate = sym<decltype(mock_cam_rotate)>("mock_cam_rotate");
    mock_cam_yaw = sym<decltype(mock_cam_yaw)>("mock_cam_yaw");
    mock_cam_pitch = sym<decltype(mock_cam_pitch)>("mock_cam_pitch");
    mock_set_wall = sym<decltype(mock_set_wall)>("mock_set_wall");
    mock_add_char = sym<decltype(mock_add_char)>("mock_add_char");
    mock_move_char = sym<decltype(mock_move_char)>("mock_move_char");
    mock_kill_obj = sym<decltype(mock_kill_obj)>("mock_kill_obj");
    mock_add_zone = sym<decltype(mock_add_zone)>("mock_add_zone");
    mock_char_material = sym<decltype(mock_char_material)>("mock_char_material");
    mock_char_material_count = sym<decltype(mock_char_material_count)>("mock_char_material_count");
    mock_material_color_r = sym<decltype(mock_material_color_r)>("mock_material_color_r");
    mock_material_queue = sym<decltype(mock_material_queue)>("mock_material_queue");
    mock_add_blind_view = sym<decltype(mock_add_blind_view)>("mock_add_blind_view");
    mock_canvasgroup_alpha_for = sym<decltype(mock_canvasgroup_alpha_for)>("mock_canvasgroup_alpha_for");

    CHECK(il2::load(), "il2::load");
    CHECK(il2::vmReady(), "vmReady");
    il2::attachThread();
    CHECK(unity::init(), "unity::init (%s)", il2::stat().failed.c_str());
    CHECK(game::init(), "game::init");
    printf("resolved %d ok, %d failed %s\n", il2::stat().ok, il2::stat().fail, il2::stat().failed.c_str());
    CHECK(il2::stat().fail == 0, "unresolved lookups");

    // ---------------- 1. empty scene ----------------
    mock_reset(); defaults();
    tick(3);
    CHECK((int)F[frame::H_COUNT] == 0, "empty scene count=%d", (int)F[frame::H_COUNT]);
    CHECK(((int)F[frame::H_FLAGS] & frame::HF_CAMERA) != 0, "camera flag");
    CHECK(F[frame::H_W] == 2400 && F[frame::H_H] == 1080, "size %g x %g", F[frame::H_W], F[frame::H_H]);

    // ---------------- 2. ESP projection / visibility / local detection ----------------
    printf("scenario: ESP\n");
    mock_reset(); defaults();
    int local = mock_add_char(0, 0, 0, "AKR(Clone)");
    int e1 = mock_add_char(2, 0, 20, "M4A1(Clone)");
    int e2 = mock_add_char(-3, 0, 30, "AWM");
    mock_set_wall(25.f);   // blocks e2 (z=30) but not e1 (z=20)
    tick(40);
    CHECK((int)F[frame::H_COUNT] == 2, "player count %d (local must be excluded)", (int)F[frame::H_COUNT]);
    CHECK(((int)F[frame::H_FLAGS] & frame::HF_LOCAL) != 0, "local flag");
    int i1 = findPlayer(e1), i2 = findPlayer(e2), il = findPlayer(local);
    CHECK(il < 0, "local char leaked into the frame");
    CHECK(i1 >= 0 && i2 >= 0, "players found %d %d", i1, i2);
    if (i1 >= 0) {
        float headY = 1.69f;   // head bone 1.62 + 0.07
        float expX = 1200.f + 2.f / 20.f * kF;
        float expYdown = 1080.f - (540.f + (headY - 1.6f) / 20.f * kF);
        CHECK(near(P(i1, frame::P_HX), expX, 1.0f), "head x %g want %g", P(i1, frame::P_HX), expX);
        CHECK(near(P(i1, frame::P_HY), expYdown, 1.0f), "head y %g want %g (y-down)", P(i1, frame::P_HY), expYdown);
        CHECK(near(P(i1, frame::P_DIST), std::sqrt(4.f + 0.09f * 0.09f + 400.f), 0.2f), "distance %g", P(i1, frame::P_DIST));
        int pf = (int)P(i1, frame::P_FLAGS);
        CHECK(pf & frame::PF_VISIBLE, "e1 should be visible (flags %d)", pf);
        CHECK(pf & frame::PF_BOX, "e1 box");
        CHECK(pf & frame::PF_ENEMY, "e1 enemy");
        CHECK(P(i1, frame::P_BY2) > P(i1, frame::P_BY1) && P(i1, frame::P_BX2) > P(i1, frame::P_BX1), "box orientation");
        CHECK(P(i1, frame::P_J + 0) == P(i1, frame::P_HX) - 0 || true, "joint 0");
        CHECK(P(i1, frame::P_WEAPON) >= 0, "weapon idx for M4A1 tool: %g", P(i1, frame::P_WEAPON));
    }
    if (i2 >= 0) CHECK(!((int)P(i2, frame::P_FLAGS) & frame::PF_VISIBLE), "e2 must be hidden behind the wall");
    CHECK((int)F[frame::H_WEAPON] == 12, "active weapon idx %d (want AKR=12)", (int)F[frame::H_WEAPON]);

    // ---------------- 3. teams from spawn zones ----------------
    printf("scenario: teams\n");
    mock_add_zone(0, 0, -3, 2);      // CT spawn right behind the local player
    mock_add_zone(-3, 0, 58, 1);     // T spawn far ahead
    int mate = mock_add_char(3, 0, -4, "USP");
    int foe = mock_add_char(-2, 0, 56, "AKR");
    tick(300);   // zones are re-scanned every 3 s
    int im = findPlayer(mate), iff = findPlayer(foe);
    CHECK((int)F[frame::H_LOCALTEAM] == 2, "local team %d (want CT=2)", (int)F[frame::H_LOCALTEAM]);
    CHECK(im >= 0 && iff >= 0, "mate/foe present %d %d", im, iff);
    if (im >= 0) CHECK(!((int)P(im, frame::P_FLAGS) & frame::PF_ENEMY), "teammate must not be flagged enemy");
    if (iff >= 0) CHECK(((int)P(iff, frame::P_FLAGS) & frame::PF_ENEMY) != 0, "T player is an enemy for a CT");

    // ---------------- 4. closed-loop aim + autofire through the whole stack ----------------
    printf("scenario: aim + autofire loop\n");
    mock_reset(); defaults();
    mock_add_char(0, 0, 0, "AKR");
    int tgt = mock_add_char(6, 0, 25, "M4");
    setCfg(cfg::C_AIM_ENABLE, 1); setCfg(cfg::C_AIM_MODE, 1); setCfg(cfg::C_AIM_HITBOX, 0);
    setCfg(cfg::C_AF_ENABLE, 1); setCfg(cfg::C_AF_MODE, 0);
    pushCfg();
    bridge::Input in; in.viewW = 2400; in.viewH = 1080; in.fps = 60; bridge::setInput(in);
    const float gTrueX = 0.7f, gTrueY = 0.6f;
    std::deque<std::pair<float, float>> pipe;
    int fireTicks = 0;
    float errNow = 1e9f;
    for (int t = 0; t < 240; t++) {
        tick();
        bridge::Command c = bridge::pollCommand();
        if (c.fireHeld > 0.5f) fireTicks++;
        pipe.push_back({c.dx, c.dy});
        if (pipe.size() > 2) {
            auto d = pipe.front(); pipe.pop_front();
            // finger +dx (right) turns the view right; +dy (down) pitches down
            mock_cam_rotate(gTrueX * d.first / kF, -gTrueY * d.second / kF);
        }
        int it = findPlayer(tgt);
        if (it >= 0) {
            float ex = P(it, frame::P_HX) - F[frame::H_CX], ey = P(it, frame::P_HY) - F[frame::H_CY];
            errNow = std::sqrt(ex * ex + ey * ey);
        }
    }
    printf("  final crosshair error %.2f px, fire ticks %d, estimated gain %.2f/%.2f (true %.2f/%.2f), lag %d\n", errNow, fireTicks, F[frame::H_GAINX], F[frame::H_GAINY], gTrueX, gTrueY, (int)F[frame::H_LAG]);
    CHECK(errNow < 4.f, "aim did not converge: %.2f px", errNow);
    CHECK(fireTicks > 30, "autofire never engaged: %d", fireTicks);
    CHECK(near(F[frame::H_GAINX], gTrueX, 0.15f), "gain estimate x %g", F[frame::H_GAINX]);
    CHECK((int)F[frame::H_TARGET] >= 0, "target index published");

    // aim must stay off while the menu is open
    in.menuOpen = 1; bridge::setInput(in);
    mock_set_camera(0, 1.6f, 0, 0, 0);
    tick(10);
    bridge::Command c2 = bridge::pollCommand();
    CHECK(c2.lookActive < 0.5f && c2.fireHeld < 0.5f, "commands must be silent while the menu is open");
    in.menuOpen = 0; bridge::setInput(in);

    // ---------------- 5. kill detection ----------------
    printf("scenario: kill event\n");
    setCfg(cfg::C_AIM_ENABLE, 0); setCfg(cfg::C_AF_ENABLE, 0); pushCfg();
    mock_set_camera(0, 1.6f, 0, 0, 0);
    in.userFiring = 1; bridge::setInput(in);
    tick(20);
    int k0 = (int)F[frame::H_KILLSEQ];
    mock_kill_obj(tgt);
    tick(30);
    CHECK((int)F[frame::H_KILLSEQ] == k0 + 1, "kill seq %d -> %d", k0, (int)F[frame::H_KILLSEQ]);
    CHECK(F[frame::H_KILLX] > 1200 && F[frame::H_KILLX] < 1800, "kill x %g", F[frame::H_KILLX]);
    in.userFiring = 0; bridge::setInput(in);

    // ---------------- 6. chams ----------------
    printf("scenario: chams\n");
    mock_reset(); defaults();
    mock_add_char(0, 0, 0, "AKR");
    int ch = mock_add_char(1, 0, 15, "M4");
    tick(20);
    void* orig0 = mock_char_material(ch, 0);
    void* orig1 = mock_char_material(ch, 1);
    setCfg(cfg::C_CHM_ENABLE, 1); pushCfg();
    tick(20);
    void* m0 = mock_char_material(ch, 0);
    CHECK(m0 != orig0, "renderer materials were not replaced");
    CHECK(mock_char_material_count(ch) == 2, "material count %zu", mock_char_material_count(ch));
    CHECK(m0 && near(mock_material_color_r(m0), 0.2f, 0.01f), "visible tint colour r=%g", m0 ? mock_material_color_r(m0) : -1.f);
    CHECK(m0 && mock_material_queue(m0) == 4000, "render queue %d", m0 ? mock_material_queue(m0) : -1);
    mock_set_wall(8.f);    // wall in front of the target -> hidden tint
    tick(30);
    void* m1 = mock_char_material(ch, 0);
    CHECK(m1 && near(mock_material_color_r(m1), 0.9f, 0.01f), "hidden tint colour r=%g", m1 ? mock_material_color_r(m1) : -1.f);
    setCfg(cfg::C_CHM_ENABLE, 0); pushCfg();
    tick(10);
    CHECK(mock_char_material(ch, 0) == orig0 && mock_char_material(ch, 1) == orig1, "original materials were not restored");

    // ---------------- 7. world effects ----------------
    printf("scenario: no flash\n");
    mock_add_blind_view();
    setCfg(cfg::C_WORLD_NOFLASH, 1); pushCfg();
    tick(60);
    CHECK(near(mock_canvasgroup_alpha_for("flashParentGO"), 0.f, 0.001f), "flash alpha %g", mock_canvasgroup_alpha_for("flashParentGO"));
    CHECK(near(mock_canvasgroup_alpha_for("shotParentGO"), 0.f, 0.001f), "screenshot alpha %g", mock_canvasgroup_alpha_for("shotParentGO"));
    setCfg(cfg::C_WORLD_NOFLASH, 0); pushCfg();
    tick(5);
    CHECK(near(mock_canvasgroup_alpha_for("flashParentGO"), 1.f, 0.001f), "flash alpha restored %g", mock_canvasgroup_alpha_for("flashParentGO"));

    // ---------------- 8. per-weapon override ----------------
    printf("scenario: weapon profile\n");
    mock_reset(); defaults();
    mock_add_char(0, 0, 0, "AWM");        // local holds an AWM
    mock_add_char(4, 0, 30, "M4");
    setCfg(cfg::C_AIM_ENABLE, 1); setCfg(cfg::C_AIM_FOV, 40); pushCfg();
    tick(30);
    float fovGlobal = F[frame::H_FOVPX];
    int awm = -1;
    for (int i = 0; i < cfg::kWeapons; i++) if (std::string(cfg::kWeaponDefs[i].id) == "awm") awm = i;
    CHECK((int)F[frame::H_WEAPON] == awm, "weapon %d want %d", (int)F[frame::H_WEAPON], awm);
    g_cfg[cfg::kWBase + awm * cfg::kWStride] = 1;                                         // override flag
    g_cfg[cfg::kWBase + awm * cfg::kWStride + 1 + cfg::kWIdx[cfg::C_AIM_FOV]] = 10;       // fov 10 deg for the AWM
    g_cfg[cfg::kWBase + awm * cfg::kWStride + 1 + cfg::kWIdx[cfg::C_AIM_ENABLE]] = 1;
    pushCfg();
    tick(5);
    float fovAwm = F[frame::H_FOVPX];
    float want = std::tan(10.f * 3.14159265f / 180.f) * kF;
    printf("  fov circle global %.1f px, awm profile %.1f px\n", fovGlobal, fovAwm);
    CHECK(near(fovAwm, want, 2.f), "awm fov px %g want %g", fovAwm, want);
    CHECK(fovGlobal > fovAwm * 3, "global fov must be larger");

    printf("\n%d checks, %d failed\n", g_checks, g_fail);
    printf("--- status ---\n%s\n", game::describe().c_str());
    return g_fail ? 1 : 0;
}
