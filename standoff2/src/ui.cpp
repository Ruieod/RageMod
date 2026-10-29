#include "ui.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <time.h>
#include <cstdio>

#include "backends/imgui_impl_opengl3.h"
#include "config.h"
#include "mods.h"
#include "game.h"
#include "hook.h"
#include "il2cpp.h"
#include "imgui.h"
#include "log.h"
#include "touch.h"

namespace ui {
namespace {

EGLBoolean (*o_swap)(EGLDisplay, EGLSurface) = nullptr;
bool g_imgui_ready = false;
bool g_menu_open = true;
bool g_touch_down = false;
game::Frame g_frame;
double g_last_time = 0.0;

ImU32 col(float r, float g, float b, float a) { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)); }

double now_s() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void init_imgui() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.FontGlobalScale = 1.6f;
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 6.f;
    st.FrameRounding = 4.f;
    st.GrabRounding = 4.f;
    st.ScrollbarSize = 26.f;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.94f);
    st.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.55f, 0.08f, 0.08f, 1.f);
    st.Colors[ImGuiCol_Header] = ImVec4(0.45f, 0.07f, 0.07f, 0.85f);
    st.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.65f, 0.12f, 0.12f, 0.9f);
    st.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.22f, 1.f);
    st.Colors[ImGuiCol_CheckMark] = ImVec4(1.f, 0.25f, 0.25f, 1.f);
    st.Colors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.2f, 0.2f, 1.f);
    st.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.f);
    st.Colors[ImGuiCol_TabSelected] = ImVec4(0.5f, 0.08f, 0.08f, 1.f);
    st.ScaleAllSizes(1.4f);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    g_imgui_ready = true;
    LOGI("ui: imgui готов");
}

void pump_input() {
    ImGuiIO& io = ImGui::GetIO();
    touch::Event e{};
    while (touch::poll(&e)) {
        switch (e.type) {
            case touch::Event::MenuToggle:
                g_menu_open = !g_menu_open;
                break;
            case touch::Event::Down:
                io.AddMousePosEvent(e.x, e.y);
                io.AddMouseButtonEvent(0, true);
                g_touch_down = true;
                break;
            case touch::Event::Move:
                io.AddMousePosEvent(e.x, e.y);
                break;
            case touch::Event::Up:
                io.AddMousePosEvent(e.x, e.y);
                io.AddMouseButtonEvent(0, false);
                g_touch_down = false;
                break;
        }
    }
}

void draw_box(ImDrawList* dl, const game::PlayerView& p, float sx, float sy, ImU32 c) {
    float h = (p.feet.y - p.top.y) * sy;
    float w = p.box_w * sx;
    if (h < 4.f || w < 2.f) return;
    float cx = p.feet.x * sx;
    float top = p.top.y * sy;
    ImVec2 a(cx - w * 0.5f, top), b(cx + w * 0.5f, top + h);
    dl->AddRect(a, b, col(0, 0, 0, 0.6f), 0.f, 0, 3.f);
    dl->AddRect(a, b, c, 0.f, 0, 1.6f);
}

void draw_bones(ImDrawList* dl, const game::PlayerView& p, float sx, float sy, ImU32 c) {
    // скелет рисуем парами по HumanBodyBones, которые реально приходят в хитбоксах
    static const int kLinks[][2] = {
        {(int)off::Bone::Head, (int)off::Bone::Neck},
        {(int)off::Bone::Neck, (int)off::Bone::Chest},
        {(int)off::Bone::Chest, (int)off::Bone::Spine},
        {(int)off::Bone::Spine, (int)off::Bone::Hips},
        {(int)off::Bone::Chest, (int)off::Bone::LeftUpperArm},
        {(int)off::Bone::LeftUpperArm, (int)off::Bone::LeftLowerArm},
        {(int)off::Bone::LeftLowerArm, (int)off::Bone::LeftHand},
        {(int)off::Bone::Chest, (int)off::Bone::RightUpperArm},
        {(int)off::Bone::RightUpperArm, (int)off::Bone::RightLowerArm},
        {(int)off::Bone::RightLowerArm, (int)off::Bone::RightHand},
        {(int)off::Bone::Hips, (int)off::Bone::LeftUpperLeg},
        {(int)off::Bone::LeftUpperLeg, (int)off::Bone::LeftLowerLeg},
        {(int)off::Bone::LeftLowerLeg, (int)off::Bone::LeftFoot},
        {(int)off::Bone::Hips, (int)off::Bone::RightUpperLeg},
        {(int)off::Bone::RightUpperLeg, (int)off::Bone::RightLowerLeg},
        {(int)off::Bone::RightLowerLeg, (int)off::Bone::RightFoot},
    };
    auto find = [&](int bone) -> const game::BoneView* {
        for (int i = 0; i < p.bone_count; i++) {
            if (p.bones[i].bone == bone && p.bones[i].on_screen) return &p.bones[i];
        }
        return nullptr;
    };
    for (const auto& l : kLinks) {
        const game::BoneView* a = find(l[0]);
        const game::BoneView* b = find(l[1]);
        if (!a || !b) continue;
        dl->AddLine(ImVec2(a->screen.x * sx, a->screen.y * sy),
                    ImVec2(b->screen.x * sx, b->screen.y * sy), c, 1.4f);
    }
    if (const game::BoneView* head = find((int)off::Bone::Head)) {
        float r = (p.feet.y - p.top.y) * sy * 0.07f;
        if (r > 1.5f) dl->AddCircle(ImVec2(head->screen.x * sx, head->screen.y * sy), r, c, 0, 1.4f);
    }
}

void draw_health(ImDrawList* dl, const game::PlayerView& p, float sx, float sy) {
    float h = (p.feet.y - p.top.y) * sy;
    float w = p.box_w * sx;
    if (h < 6.f) return;
    float x = p.feet.x * sx - w * 0.5f - 6.f;
    float top = p.top.y * sy;
    float frac = (float)p.hp / (float)p.maxhp;
    frac = frac < 0.f ? 0.f : (frac > 1.f ? 1.f : frac);
    dl->AddRectFilled(ImVec2(x - 3.f, top - 1.f), ImVec2(x, top + h + 1.f), col(0, 0, 0, 0.7f));
    dl->AddRectFilled(ImVec2(x - 3.f, top + h * (1.f - frac)), ImVec2(x, top + h),
                      col(1.f - frac, frac, 0.15f, 1.f));
}

void draw_esp() {
    if (!g_cfg.esp || g_frame.count == 0) return;
    ImGuiIO& io = ImGui::GetIO();
    if (g_frame.screen_w <= 0 || g_frame.screen_h <= 0) return;
    float sx = io.DisplaySize.x / (float)g_frame.screen_w;
    float sy = io.DisplaySize.y / (float)g_frame.screen_h;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (int i = 0; i < g_frame.count; i++) {
        const game::PlayerView& p = g_frame.players[i];
        if (!p.on_screen) continue;
        if (g_cfg.esp_team_check && !p.enemy) continue;
        if (g_cfg.esp_visible_only && !p.visible) continue;
        if (p.dist > g_cfg.esp_max_dist) continue;

        ImU32 c = p.enemy ? (p.visible ? col(1.f, 0.25f, 0.25f, 0.95f) : col(0.75f, 0.35f, 0.35f, 0.8f))
                          : col(0.3f, 0.8f, 1.f, 0.9f);

        if (g_cfg.esp_box) draw_box(dl, p, sx, sy, c);
        if (g_cfg.esp_bones) draw_bones(dl, p, sx, sy, c);
        if (g_cfg.esp_health) draw_health(dl, p, sx, sy);
        if (g_cfg.esp_snaplines) {
            dl->AddLine(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y),
                        ImVec2(p.feet.x * sx, p.feet.y * sy), c, 1.2f);
        }
        if (g_cfg.esp_dist) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%.0fm  %d hp", p.dist, p.hp);
            ImVec2 at(p.feet.x * sx, p.feet.y * sy + 2.f);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            at.x -= ts.x * 0.5f;
            dl->AddText(ImVec2(at.x + 1.f, at.y + 1.f), col(0, 0, 0, 0.9f), buf);
            dl->AddText(at, c, buf);
        }
    }

    if (g_cfg.draw_fov) {
        float r = io.DisplaySize.y * 0.5f * std::tan(g_cfg.aim_fov * 0.0174533f) /
                  std::tan(g_frame.fov * 0.5f * 0.0174533f);
        dl->AddCircle(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), r,
                      col(1.f, 1.f, 1.f, 0.35f), 64, 1.4f);
    }
    if (g_frame.has_target && (g_cfg.aim_silent || g_cfg.aim_visible)) {
        dl->AddCircleFilled(ImVec2(g_frame.target_screen.x * sx, g_frame.target_screen.y * sy), 4.f,
                            col(1.f, 0.9f, 0.2f, 0.9f));
    }
}

void draw_menu() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(12.f, 12.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(170.f, 0.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("rage##chip", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoScrollbar)) {
        if (ImGui::Button(g_menu_open ? "RAGE  [-]" : "RAGE  [+]", ImVec2(140.f, 0.f))) {
            g_menu_open = !g_menu_open;
        }
    }
    ImGui::End();

    if (!g_menu_open) return;

    ImGui::SetNextWindowPos(ImVec2(12.f, 90.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.42f, io.DisplaySize.y * 0.62f),
                             ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("RAGE // standoff2 1.0.0", &g_menu_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("ESP", &g_cfg.esp);
            ImGui::Checkbox("Box", &g_cfg.esp_box);
            ImGui::Checkbox("Skeleton", &g_cfg.esp_bones);
            ImGui::Checkbox("Health", &g_cfg.esp_health);
            ImGui::Checkbox("Distance / HP", &g_cfg.esp_dist);
            ImGui::Checkbox("Snaplines", &g_cfg.esp_snaplines);
            ImGui::Checkbox("Only enemies", &g_cfg.esp_team_check);
            ImGui::Checkbox("Only visible", &g_cfg.esp_visible_only);
            ImGui::SliderFloat("Max dist", &g_cfg.esp_max_dist, 10.f, 300.f, "%.0f m");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AIMBOT")) {
            ImGui::Checkbox("Silent aim", &g_cfg.aim_silent);
            ImGui::TextDisabled("камера не двигается, правится сам хит-каст");
            const char* modes[] = {"hit transform", "ray override"};
            ImGui::Combo("Silent mode", &g_cfg.aim_silent_mode, modes, 2);
            ImGui::Separator();
            ImGui::Checkbox("Visible aim (net angles)", &g_cfg.aim_visible);
            ImGui::SliderFloat("Smooth", &g_cfg.aim_smooth, 1.f, 25.f, "%.1f");
            const char* quats[] = {"-", "quat A", "quat B", "both"};
            ImGui::Combo("Write", &g_cfg.aim_quat, quats, 4);
            ImGui::Separator();
            const char* bones[] = {"Head", "Neck", "Chest", "Spine", "Hips"};
            static const int bone_ids[] = {10, 9, 8, 7, 0};
            int sel = 0;
            for (int i = 0; i < 5; i++) {
                if (bone_ids[i] == g_cfg.aim_bone) sel = i;
            }
            if (ImGui::Combo("Bone", &sel, bones, 5)) g_cfg.aim_bone = bone_ids[sel];
            ImGui::Checkbox("Body fallback", &g_cfg.aim_fallback_body);
            ImGui::SliderFloat("FOV", &g_cfg.aim_fov, 1.f, 60.f, "%.1f deg");
            ImGui::SliderFloat("Max dist##aim", &g_cfg.aim_max_dist, 10.f, 300.f, "%.0f m");
            ImGui::Checkbox("Visibility check", &g_cfg.aim_visible_check);
            ImGui::Checkbox("Only enemies##aim", &g_cfg.aim_team_check);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MISC")) {
            ImGui::Checkbox("FOV circle", &g_cfg.draw_fov);
            ImGui::Checkbox("Debug overlay", &g_cfg.draw_debug);
            ImGui::Separator();
            ImGui::TextDisabled("touch mapping");
            ImGui::Checkbox("Swap XY", &g_cfg.touch_swap_xy);
            ImGui::Checkbox("Invert X", &g_cfg.touch_invert_x);
            ImGui::Checkbox("Invert Y", &g_cfg.touch_invert_y);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CONFIG")) {
            ImGui::TextWrapped("%s", config::path());
            if (ImGui::Button("Save", ImVec2(140.f, 0.f))) config::save();
            ImGui::SameLine();
            if (ImGui::Button("Load", ImVec2(140.f, 0.f))) config::load();
            ImGui::Separator();
            ImGui::Text("hooks: %d/4", mods::installed_count());
            ImGui::Text("libunity: %p", (void*)il2cpp::base());
            ImGui::Text("entities: %d, players: %d", g_frame.entities_seen, g_frame.count);
            ImGui::Text("touch: %s", touch::available() ? "ok" : "нет (root?)");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void draw_debug() {
    if (!g_cfg.draw_debug) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    char buf[160];
    snprintf(buf, sizeof(buf), "RAGE | hooks %d/4 | ent %d | plr %d | cam %.0fx%.0f | fov %.0f",
             mods::installed_count(), g_frame.entities_seen, g_frame.count,
             (float)g_frame.screen_w, (float)g_frame.screen_h, g_frame.fov);
    dl->AddText(ImVec2(9.f, 3.f), col(0, 0, 0, 0.9f), buf);
    dl->AddText(ImVec2(8.f, 2.f), col(1.f, 0.3f, 0.3f, 1.f), buf);
}

EGLBoolean hk_swap(EGLDisplay dpy, EGLSurface surf) {
    if (!g_imgui_ready) init_imgui();

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    float w = (float)vp[2], h = (float)vp[3];
    if (w < 16.f || h < 16.f) return o_swap(dpy, surf);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(w, h);
    double t = now_s();
    io.DeltaTime = g_last_time > 0.0 ? (float)(t - g_last_time) : 1.f / 60.f;
    if (io.DeltaTime <= 0.f) io.DeltaTime = 1.f / 60.f;
    g_last_time = t;
    touch::set_screen((int)w, (int)h);

    pump_input();
    game::copy_frame(&g_frame);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    draw_esp();
    draw_menu();
    draw_debug();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return o_swap(dpy, surf);
}

} // namespace

bool install() {
    if (!hook::got(il2cpp::base(), "eglSwapBuffers", (void*)hk_swap, (void**)&o_swap)) {
        LOGE("ui: не нашли слот eglSwapBuffers в libunity.so");
        return false;
    }
    return true;
}

bool menu_open() { return g_menu_open; }

} // namespace ui
