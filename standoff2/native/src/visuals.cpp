// Chams (material swap through walls) and world effects (no flash / smoke / stun).
#include <algorithm>

#include "model.h"
#include "unity.h"

namespace game {
namespace {

using il2::Ref;

Ref g_matVis, g_matHid;
bool g_matsReady = false;
float g_lastCols[8] = {};
int g_lastStyle = -1;

il2::Klass* K_BlindView = nullptr;
il2::Klass* K_StunView = nullptr;
il2::Klass* K_SmokeView = nullptr;
il2::Klass* K_SmokeMod = nullptr;
il2::Field* F_flashParent = nullptr;
il2::Field* F_shotParent = nullptr;

std::vector<Ref> g_flashCgs, g_stunCgs, g_smokeCgs;
std::vector<Ref> g_smokeRend;
bool g_flashApplied = false, g_stunApplied = false, g_smokeApplied = false;
double g_worldScanT = 0;
int g_smokeParts = 0;

bool hasTarget(const std::vector<Ref>& v, void* p) {
    for (const Ref& r : v) if (r.get() == p) return true;
    return false;
}

void addCanvasGroupOf(std::vector<Ref>& v, void* component) {
    if (!component || !unity::alive(component)) return;
    void* go = unity::Component_get_gameObject(component);
    void* cg = unity::getOrAddCanvasGroup(go);
    if (cg && !hasTarget(v, cg)) v.emplace_back(cg);
}

void ensureMaterials() {
    if (g_matsReady) return;
    void* sh = unity::shaderFind("Hidden/Internal-Colored");
    if (!sh) { static bool warned = false; if (!warned) { warned = true; rlog("chams: Hidden/Internal-Colored not found"); } return; }
    void* a = unity::newMaterial(sh);
    void* b = unity::newMaterial(sh);
    if (!a || !b) return;
    g_matVis.set(a);
    g_matHid.set(b);
    for (void* m : {a, b}) {
        unity::matInt(m, "_ZTest", 8);      // CompareFunction.Always
        unity::matInt(m, "_ZWrite", 0);
        unity::matInt(m, "_Cull", 0);
        try { unity::Material_set_renderQueue(m, 4000); } catch (...) {}
    }
    g_matsReady = true;
    rlog("chams: materials ready");
}

void refreshMaterialColors() {
    if (!g_matsReady) return;
    const int style = static_cast<int>(G(cfg::C_CHM_MODE));
    Color v = colorAt(cfg::C_CHM_CVIS), h = colorAt(cfg::C_CHM_CHID);
    float cur[8] = {v.r, v.g, v.b, v.a, h.r, h.g, h.b, h.a};
    bool same = style == g_lastStyle;
    for (int i = 0; i < 8 && same; i++) same = cur[i] == g_lastCols[i];
    if (same) return;
    g_lastStyle = style;
    for (int i = 0; i < 8; i++) g_lastCols[i] = cur[i];
    if (style == 0) { v.a = 1.f; h.a = std::max(h.a, 0.9f); }
    else { v.a *= 0.55f; h.a *= 0.55f; }
    unity::matColor(g_matVis.get(), "_Color", v);
    unity::matColor(g_matHid.get(), "_Color", h);
}

void restoreChams(Char& c) {
    if (c.chamsState == 0) return;
    void* r = c.rend.get();
    if (r && unity::alive(r) && c.origMats) {
        try { unity::Renderer_set_sharedMaterials(r, c.origMats.get()); } catch (...) {}
    }
    c.origMats.reset();
    c.chamsState = 0;
}

void applyChams(Char& c, int state) {
    void* r = c.rend.get();
    if (!r || !unity::alive(r)) return;
    try {
        if (c.chamsState == 0) {
            void* orig = unity::Renderer_get_sharedMaterials(r);
            if (!orig) return;
            c.origMats.set(orig);
        }
        size_t n = il2::arrayLen(c.origMats.get());
        if (n == 0) n = 1;
        void* mat = state == 1 ? g_matVis.get() : g_matHid.get();
        void* arr = il2::newArray(unity::K_Material, n);
        void** d = il2::arrayData(arr);
        for (size_t i = 0; i < n; i++) d[i] = mat;
        unity::Renderer_set_sharedMaterials(r, arr);
        c.chamsState = state;
    } catch (...) { c.chamsState = 0; }
}

void chamsTick() {
    const bool on = GB(cfg::C_CHM_ENABLE);
    if (on) { ensureMaterials(); refreshMaterialColors(); }
    for (auto& up : W.chars) {
        Char& c = *up;
        if (!c.rend) continue;
        const bool want = on && g_matsReady && !c.local && c.valid && (!c.teammate || GB(cfg::C_CHM_TEAM));
        if (!want) { restoreChams(c); continue; }
        const int state = c.visible ? 1 : 2;
        bool recheck = W.now - c.chamsCheck > 1.0;
        if (recheck && c.chamsState != 0) {
            // the game may re-assign materials (skin refresh, spawn protection): verify ours are still on
            try {
                void* cur = unity::Renderer_get_sharedMaterials(c.rend.get());
                void* first = il2::arrayLen(cur) ? il2::arrayData(cur)[0] : nullptr;
                void* ours = c.chamsState == 1 ? g_matVis.get() : g_matHid.get();
                if (first != ours && first != g_matVis.get() && first != g_matHid.get()) { c.origMats.reset(); c.chamsState = 0; }
            } catch (...) {}
        }
        if (recheck) c.chamsCheck = W.now;
        if (c.chamsState != state) applyChams(c, state);
    }
}

// ---- world effects ----

void scanSmokeParticles() {
    if (!K_SmokeMod || !unity::K_Renderer) return;
    void* rtype = il2::typeObject(unity::K_Renderer);
    for (void* v : unity::findAll(K_SmokeMod)) {
        try {
            void* arr = unity::Component_GetComponentsInChildren(v, rtype, true);
            size_t n = il2::arrayLen(arr);
            void** d = n ? il2::arrayData(arr) : nullptr;
            for (size_t i = 0; i < n && i < 256; i++) {
                if (d[i] && !hasTarget(g_smokeRend, d[i])) g_smokeRend.emplace_back(d[i]);
            }
        } catch (...) {}
    }
}

void scanWorld() {
    if (K_BlindView && GB(cfg::C_WORLD_NOFLASH)) {
        for (void* v : unity::findAll(K_BlindView)) {
            if (F_flashParent) addCanvasGroupOf(g_flashCgs, il2::read<void*>(v, il2::fieldOffset(F_flashParent)));
            if (F_shotParent) addCanvasGroupOf(g_flashCgs, il2::read<void*>(v, il2::fieldOffset(F_shotParent)));
        }
    }
    if (K_StunView && GB(cfg::C_WORLD_NOSTUN)) {
        for (void* v : unity::findAll(K_StunView)) addCanvasGroupOf(g_stunCgs, v);
    }
    if (GB(cfg::C_WORLD_NOSMOKE)) {
        if (K_SmokeView) for (void* v : unity::findAll(K_SmokeView)) addCanvasGroupOf(g_smokeCgs, v);
        scanSmokeParticles();
    }
}

void setAlpha(std::vector<Ref>& list, float a) {
    for (auto it = list.begin(); it != list.end();) {
        void* cg = it->get();
        if (!cg || !unity::alive(cg)) { it = list.erase(it); continue; }
        try { unity::CanvasGroup_set_alpha(cg, a); } catch (...) {}
        ++it;
    }
}

void setRenderers(bool enabled) {
    for (auto it = g_smokeRend.begin(); it != g_smokeRend.end();) {
        void* r = it->get();
        if (!r || !unity::alive(r)) { it = g_smokeRend.erase(it); continue; }
        try { unity::Renderer_set_enabled(r, enabled); } catch (...) {}
        ++it;
    }
}

void worldTick() {
    const bool flash = GB(cfg::C_WORLD_NOFLASH), stun = GB(cfg::C_WORLD_NOSTUN), smoke = GB(cfg::C_WORLD_NOSMOKE);
    if ((flash || stun || smoke) && W.now - g_worldScanT > (smoke ? 0.15 : 0.4)) { g_worldScanT = W.now; scanWorld(); }
    if (flash) { setAlpha(g_flashCgs, 0.f); g_flashApplied = true; }
    else if (g_flashApplied) { setAlpha(g_flashCgs, 1.f); g_flashCgs.clear(); g_flashApplied = false; }
    if (stun) { setAlpha(g_stunCgs, 0.f); g_stunApplied = true; }
    else if (g_stunApplied) { setAlpha(g_stunCgs, 1.f); g_stunCgs.clear(); g_stunApplied = false; }
    if (smoke) { setAlpha(g_smokeCgs, 0.f); setRenderers(false); g_smokeApplied = true; }
    else if (g_smokeApplied) { setAlpha(g_smokeCgs, 1.f); g_smokeCgs.clear(); setRenderers(true); g_smokeRend.clear(); g_smokeApplied = false; }
    g_smokeParts = static_cast<int>(g_smokeRend.size());
}

}  // namespace

bool visualsInit() {
    K_BlindView = il2::klass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.BlindEffect", "GrenadeBlindEffectView");
    K_StunView = il2::klass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades", "GrenadeStunEffectView");
    K_SmokeView = il2::klass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.Smoke", "GrenadeSmokeEffectView");
    K_SmokeMod = il2::klass("Axlebolt.Standoff.Gameplay.Grenade.GrenadeDetonation.Smoke.Components.SmokeEffectModifier", "SmokeEffectModifiersView");
    F_flashParent = il2::field("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.BlindEffect", "GrenadeBlindEffectView", "_flashEffectParent");
    F_shotParent = il2::field("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.BlindEffect", "GrenadeBlindEffectView", "_screenshotParent");
    rlog("visuals: blind=%d stun=%d smoke2d=%d smoke3d=%d", K_BlindView != nullptr, K_StunView != nullptr, K_SmokeView != nullptr, K_SmokeMod != nullptr);
    return true;
}

void visualsTick() {
    try { chamsTick(); } catch (...) {}
    try { worldTick(); } catch (...) {}
}

std::string visualsDescribe() {
    char b[160];
    snprintf(b, sizeof b, "chams mats=%d  flashCG=%zu stunCG=%zu smokeCG=%zu smokePS=%d", g_matsReady ? 1 : 0, g_flashCgs.size(), g_stunCgs.size(), g_smokeCgs.size(), g_smokeParts);
    return b;
}

}  // namespace game
