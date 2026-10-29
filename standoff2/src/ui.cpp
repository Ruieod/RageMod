#include "ui.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <time.h>

#include <atomic>
#include <cstdio>

#include "backends/imgui_impl_opengl3.h"
#include "config.h"
#include "game.h"
#include "hook.h"
#include "il2cpp.h"
#include "imgui.h"
#include "log.h"
#include "mods.h"
#include "roboto_font.h"
#include "touch.h"

namespace ui {
namespace {

// ---- палитра и метрики, снято с референса ----
namespace th {
const ImU32 bg_window  = IM_COL32(23, 25, 28, 250);
const ImU32 bg_header  = IM_COL32(17, 19, 22, 255);
const ImU32 bg_side    = IM_COL32(19, 21, 24, 255);
const ImU32 panel      = IM_COL32(30, 33, 38, 255);
const ImU32 panel_line = IM_COL32(44, 48, 54, 255);
const ImU32 control    = IM_COL32(38, 42, 48, 255);
const ImU32 text       = IM_COL32(206, 211, 217, 255);
const ImU32 text_dim   = IM_COL32(118, 126, 136, 255);
const ImU32 text_mute  = IM_COL32(86, 93, 102, 255);
const ImU32 accent     = IM_COL32(61, 124, 253, 255);
const ImU32 accent_bg  = IM_COL32(61, 124, 253, 38);
const ImU32 shadow     = IM_COL32(0, 0, 0, 110);
} // namespace th

EGLBoolean (*o_swap)(EGLDisplay, EGLSurface) = nullptr;
bool g_imgui_ready = false;
bool g_menu_open = false;
std::atomic<bool> g_wants_input{false};
std::atomic<unsigned long long> g_frames{0};
game::Frame g_frame;
double g_last_time = 0.0;
float S = 1.f; // масштаб интерфейса
int g_tab = 0;
float g_fps = 0.f;

enum Icon { IcoRage, IcoLegit, IcoTrigger, IcoVisual, IcoWorld, IcoMisc, IcoConfig };

struct Tab {
    const char* name;
    Icon icon;
};
const Tab kTabs[] = {
    {"Ragebot", IcoRage}, {"Legitbot", IcoLegit}, {"Trigger", IcoTrigger}, {"Visuals", IcoVisual},
    {"World", IcoWorld},  {"Misc", IcoMisc},      {"Configs", IcoConfig},
};
constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));

double now_s() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// ---- иконки рисуем вручную: шрифта с глифами нет ----
void draw_icon(ImDrawList* dl, ImVec2 c, float r, Icon id, ImU32 col) {
    const float t = 1.4f * S;
    switch (id) {
        case IcoRage:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddLine(ImVec2(c.x - r - t * 2.f, c.y), ImVec2(c.x - r * 0.4f, c.y), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.4f, c.y), ImVec2(c.x + r + t * 2.f, c.y), col, t);
            dl->AddLine(ImVec2(c.x, c.y - r - t * 2.f), ImVec2(c.x, c.y - r * 0.4f), col, t);
            dl->AddLine(ImVec2(c.x, c.y + r * 0.4f), ImVec2(c.x, c.y + r + t * 2.f), col, t);
            break;
        case IcoLegit:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddCircleFilled(c, r * 0.32f, col);
            break;
        case IcoTrigger:
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.6f), ImVec2(c.x + r * 0.2f, c.y - r), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.2f, c.y - r), ImVec2(c.x - r * 0.1f, c.y), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.1f, c.y), ImVec2(c.x + r, c.y - r * 0.2f), col, t);
            break;
        case IcoVisual:
            dl->PathArcTo(ImVec2(c.x, c.y + r * 1.1f), r * 1.5f, -2.4f, -0.75f, 16);
            dl->PathStroke(col, 0, t);
            dl->PathArcTo(ImVec2(c.x, c.y - r * 1.1f), r * 1.5f, 0.75f, 2.4f, 16);
            dl->PathStroke(col, 0, t);
            dl->AddCircleFilled(c, r * 0.35f, col);
            break;
        case IcoWorld:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, t);
            dl->AddEllipse(c, ImVec2(r * 0.5f, r), col, 0.f, 0, t);
            break;
        case IcoMisc:
            dl->AddCircle(c, r * 0.5f, col, 0, t);
            for (int i = 0; i < 6; i++) {
                float a = (float)i * 1.0471976f;
                dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.7f, c.y + std::sin(a) * r * 0.7f),
                            ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r), col, t);
            }
            break;
        case IcoConfig:
            dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.f * S, 0, t);
            dl->AddRectFilled(ImVec2(c.x - r * 0.45f, c.y - r), ImVec2(c.x + r * 0.45f, c.y - r * 0.25f), col);
            break;
    }
}

// ---- панель-группа: фон уходит в отдельный канал, высота известна только в конце ----
ImDrawListSplitter g_split;
ImVec2 g_group_start;
bool g_in_group = false;

void group_begin(const char* title) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    g_split.Split(dl, 2);
    g_split.SetCurrentChannel(dl, 1);
    g_in_group = true;

    g_group_start = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(0.f, 9.f * S));
    ImGui::Indent(12.f * S);
    dl->AddText(ImGui::GetCursorScreenPos(), th::text, title);
    ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 6.f * S));
}

void group_end() {
    if (!g_in_group) return;
    ImGui::Unindent(12.f * S);
    ImGui::Dummy(ImVec2(0.f, 9.f * S));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2.f;
    ImVec2 p1(g_group_start.x + w, ImGui::GetCursorScreenPos().y);
    g_split.SetCurrentChannel(dl, 0);
    dl->AddRectFilled(g_group_start, p1, th::panel, 6.f * S);
    dl->AddRect(g_group_start, p1, th::panel_line, 6.f * S, 0, 1.f);
    g_split.Merge(dl);
    g_in_group = false;
    ImGui::Dummy(ImVec2(0.f, 10.f * S));
}

float row_h() { return 22.f * S; }
float row_w() { return ImGui::GetContentRegionAvail().x - 12.f * S; }

// ---- строки-виджеты ----
bool checkbox_row(const char* label, bool* v) {
    ImGui::PushID(label);
    const float h = row_h();
    const float w = row_w();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##r", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool changed = ImGui::IsItemClicked();
    if (changed) *v = !*v;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    dl->AddText(ImVec2(p.x, p.y + (h - fs) * 0.5f), (*v || hovered) ? th::text : th::text_dim, label);

    float box = 13.f * S;
    ImVec2 b0(p.x + w - box, p.y + (h - box) * 0.5f);
    ImVec2 b1(b0.x + box, b0.y + box);
    if (*v) {
        dl->AddRectFilled(b0, b1, th::accent, 3.f * S);
        dl->AddLine(ImVec2(b0.x + box * 0.24f, b0.y + box * 0.52f), ImVec2(b0.x + box * 0.44f, b0.y + box * 0.74f),
                    IM_COL32_WHITE, 1.8f * S);
        dl->AddLine(ImVec2(b0.x + box * 0.44f, b0.y + box * 0.74f), ImVec2(b0.x + box * 0.78f, b0.y + box * 0.28f),
                    IM_COL32_WHITE, 1.8f * S);
    } else {
        dl->AddRectFilled(b0, b1, th::control, 3.f * S);
        dl->AddRect(b0, b1, hovered ? th::accent : th::panel_line, 3.f * S, 0, 1.f);
    }
    ImGui::PopID();
    return changed;
}

bool combo_row(const char* label, int* v, const char* const* items, int count) {
    ImGui::PushID(label);
    const float h = row_h();
    const float w = row_w();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##r", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##pop");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    dl->AddText(ImVec2(p.x, p.y + (h - fs) * 0.5f), th::text_dim, label);

    const char* cur = (*v >= 0 && *v < count) ? items[*v] : "-";
    ImVec2 ts = ImGui::CalcTextSize(cur);
    dl->AddText(ImVec2(p.x + w - ts.x - 11.f * S, p.y + (h - fs) * 0.5f), hovered ? th::accent : th::text, cur);
    ImVec2 c(p.x + w - 4.f * S, p.y + h * 0.5f);
    dl->AddTriangleFilled(ImVec2(c.x - 4.f * S, c.y - 1.5f * S), ImVec2(c.x, c.y + 3.f * S),
                          ImVec2(c.x + 4.f * S, c.y - 1.5f * S), th::text_dim);

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(p.x, p.y + h));
    if (ImGui::BeginPopup("##pop")) {
        for (int i = 0; i < count; i++) {
            if (ImGui::Selectable(items[i], i == *v)) {
                *v = i;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool slider_row(const char* label, float* v, float mn, float mx, const char* fmt) {
    ImGui::PushID(label);
    const float w = row_w();
    const float fs = ImGui::GetFontSize();
    const float h = fs + 15.f * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##r", ImVec2(w, h));
    bool active = ImGui::IsItemActive();

    float t = (mx > mn) ? (*v - mn) / (mx - mn) : 0.f;
    bool changed = false;
    if (active) {
        float nt = (ImGui::GetIO().MousePos.x - p.x) / (w > 1.f ? w : 1.f);
        nt = nt < 0.f ? 0.f : (nt > 1.f ? 1.f : nt);
        float nv = mn + nt * (mx - mn);
        if (nv != *v) {
            *v = nv;
            changed = true;
        }
        t = nt;
    }
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, th::text_dim, label);
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, *v);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(p.x + w - ts.x, p.y), active ? th::accent : th::text, buf);

    float ty = p.y + fs + 8.f * S;
    float tr = 2.5f * S;
    dl->AddRectFilled(ImVec2(p.x, ty - tr), ImVec2(p.x + w, ty + tr), th::control, tr);
    dl->AddRectFilled(ImVec2(p.x, ty - tr), ImVec2(p.x + w * t, ty + tr), th::accent, tr);
    dl->AddCircleFilled(ImVec2(p.x + w * t, ty), 5.f * S, IM_COL32_WHITE);
    ImGui::PopID();
    return changed;
}

bool slider_int_row(const char* label, int* v, int mn, int mx) {
    float f = (float)*v;
    bool ch = slider_row(label, &f, (float)mn, (float)mx, "%.0f");
    if (ch) *v = (int)(f + 0.5f);
    return ch;
}

bool wide_button(const char* label) {
    const float w = row_w();
    const float h = 26.f * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::InvisibleButton("##b", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), hovered ? th::accent : th::control, 4.f * S);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), hovered ? IM_COL32_WHITE : th::text, label);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.f, 5.f * S));
    return clicked;
}

void hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(th::text_mute));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + row_w());
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.f, 2.f * S));
}

void info_row(const char* label, const char* value, ImU32 col) {
    const float h = row_h();
    const float w = row_w();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    dl->AddText(ImVec2(p.x, p.y + (h - fs) * 0.5f), th::text_dim, label);
    ImVec2 ts = ImGui::CalcTextSize(value);
    dl->AddText(ImVec2(p.x + w - ts.x, p.y + (h - fs) * 0.5f), col, value);
}

// ---- вкладки: col 0 — левая колонка, col 1 — правая ----
void tab_ragebot(int col) {
    if (col == 0) {
        group_begin("Main");
        checkbox_row("Enabled", &g_cfg.aim_silent);
        static const char* modes[] = {"Hit transform", "Ray override"};
        combo_row("Mode", &g_cfg.aim_silent_mode, modes, 2);
        checkbox_row("Visibility check", &g_cfg.aim_visible_check);
        checkbox_row("Enemies only", &g_cfg.aim_team_check);
        group_end();

        group_begin("Accuracy");
        slider_row("Field of view", &g_cfg.aim_fov, 1.f, 60.f, "%.0f");
        slider_row("Max distance", &g_cfg.aim_max_dist, 10.f, 300.f, "%.0f m");
        group_end();
        return;
    }

    group_begin("Selection");
    static const char* bones[] = {"Head", "Neck", "Chest", "Spine", "Hips"};
    static const int bone_ids[] = {10, 9, 8, 7, 0};
    int sel = 0;
    for (int i = 0; i < 5; i++) {
        if (bone_ids[i] == g_cfg.aim_bone) sel = i;
    }
    if (combo_row("Hitbox", &sel, bones, 5)) g_cfg.aim_bone = bone_ids[sel];
    checkbox_row("Body fallback", &g_cfg.aim_fallback_body);
    group_end();

    group_begin("Target");
    char buf[48];
    snprintf(buf, sizeof(buf), "%s", g_frame.has_target ? "locked" : "none");
    info_row("State", buf, g_frame.has_target ? th::accent : th::text_dim);
    snprintf(buf, sizeof(buf), "%d", g_frame.count);
    info_row("Players", buf, th::text);
    group_end();
}

void tab_legitbot(int col) {
    if (col == 0) {
        group_begin("Main");
        checkbox_row("Enabled", &g_cfg.aim_visible);
        slider_row("Smoothing", &g_cfg.aim_smooth, 1.f, 25.f, "%.1f");
        static const char* quats[] = {"None", "Quat A", "Quat B", "Both"};
        combo_row("Write angles", &g_cfg.aim_quat, quats, 4);
        group_end();
        return;
    }
    group_begin("Safety");
    checkbox_row("Visibility check", &g_cfg.aim_visible_check);
    checkbox_row("Enemies only", &g_cfg.aim_team_check);
    hint("Углы уходят на сервер: чем выше smoothing, тем меньше похоже на снап.");
    group_end();
}

void tab_trigger(int col) {
    if (col == 0) {
        group_begin("Auto fire");
        checkbox_row("Enabled", &g_cfg.autofire);
        static const char* fmodes[] = {"Hold always", "On target"};
        combo_row("Mode", &g_cfg.autofire_mode, fmodes, 2);
        slider_row("Trigger FOV", &g_cfg.autofire_fov, 0.5f, 30.f, "%.1f");
        checkbox_row("Pulse (semi-auto)", &g_cfg.autofire_pulse);
        group_end();

        group_begin("Recoil");
        checkbox_row("No recoil", &g_cfg.no_recoil);
        slider_row("Recoil mult", &g_cfg.recoil_mult, 0.f, 1.f, "%.2f");
        group_end();
        return;
    }

    group_begin("Fire action");
    ImGui::SetNextItemWidth(row_w());
    ImGui::InputText("##action", g_cfg.fire_action, sizeof(g_cfg.fire_action));
    hint("Игра спрашивала такие экшены — ткни нужный:");
    int n = mods::seen_action_count();
    for (int i = 0; i < n; i++) {
        const char* name = mods::seen_action(i);
        if (!name[0]) continue;
        ImGui::PushID(i);
        if (ImGui::SmallButton(name)) snprintf(g_cfg.fire_action, sizeof(g_cfg.fire_action), "%s", name);
        ImGui::PopID();
    }
    if (n == 0) hint("(пусто — зайди в раунд)");
    group_end();
}

void tab_visuals(int col) {
    if (col == 0) {
        group_begin("Players");
        checkbox_row("Enabled", &g_cfg.esp);
        checkbox_row("Box", &g_cfg.esp_box);
        checkbox_row("Skeleton", &g_cfg.esp_bones);
        checkbox_row("Health bar", &g_cfg.esp_health);
        checkbox_row("Distance / HP", &g_cfg.esp_dist);
        checkbox_row("Snaplines", &g_cfg.esp_snaplines);
        group_end();
        return;
    }

    group_begin("Filters");
    checkbox_row("Enemies only", &g_cfg.esp_team_check);
    checkbox_row("Visible only", &g_cfg.esp_visible_only);
    slider_row("Max distance", &g_cfg.esp_max_dist, 10.f, 300.f, "%.0f m");
    group_end();

    group_begin("Chams");
    static const char* cmodes[] = {"Off", "Render queue", "Outline mat", "Untouchable"};
    combo_row("Mode", &g_cfg.chams, cmodes, 4);
    slider_int_row("Queue", &g_cfg.chams_queue, 2000, 5000);
    checkbox_row("Enemies only", &g_cfg.chams_team_check);
    group_end();
}

void tab_world(int col) {
    if (col == 0) {
        group_begin("Camera");
        checkbox_row("Third person", &g_cfg.third_person);
        slider_row("Distance", &g_cfg.tp_dist, 0.5f, 6.f, "%.1f m");
        slider_row("Height", &g_cfg.tp_height, -1.f, 2.f, "%.2f m");
        checkbox_row("Show own body", &g_cfg.tp_show_body);
        group_end();
        return;
    }
    group_begin("Overlay");
    checkbox_row("FOV circle", &g_cfg.draw_fov);
    checkbox_row("Watermark", &g_cfg.watermark);
    checkbox_row("Debug line", &g_cfg.draw_debug);
    group_end();
}

void tab_misc(int col) {
    if (col == 0) {
        group_begin("Input");
        static const char* sources[] = {"Game input", "/dev/input"};
        combo_row("Touch source", &g_cfg.touch_source, sources, 2);
        checkbox_row("Swallow under menu", &g_cfg.menu_swallow);
        hint("Game input не требует рута: тач читается из UnityEngine.Input.");
        group_end();

        group_begin("Interface");
        slider_row("Menu scale", &g_cfg.menu_scale, 0.7f, 2.2f, "%.2f");
        group_end();
        return;
    }
    group_begin("Touch mapping");
    hint("Только для режима /dev/input.");
    checkbox_row("Swap XY", &g_cfg.touch_swap_xy);
    checkbox_row("Invert X", &g_cfg.touch_invert_x);
    checkbox_row("Invert Y", &g_cfg.touch_invert_y);
    group_end();
}

void tab_configs(int col) {
    if (col == 0) {
        group_begin("Config");
        if (wide_button("Save")) config::save();
        if (wide_button("Load")) config::load();
        hint(config::path());
        group_end();
        return;
    }
    group_begin("Game hooks");
    hint("Хуки патчат код игры. На старте выключены: патч функции, которую в этот "
         "момент выполняет другой поток, роняет процесс. Жми, когда игра загрузилась.");
    if (!mods::installed()) {
        if (wide_button("Enable game hooks")) mods::install();
    } else {
        char hb[48];
        snprintf(hb, sizeof(hb), "%d/7", mods::installed_count());
        info_row("Installed", hb, mods::installed_count() == 7 ? th::accent : th::text);
    }
    group_end();

    group_begin("Status");
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/7", mods::installed_count());
    info_row("Hooks", buf, mods::installed_count() == 7 ? th::accent : th::text);
    snprintf(buf, sizeof(buf), "%p", (void*)il2cpp::base());
    info_row("libunity", buf, th::text);
    snprintf(buf, sizeof(buf), "%d", g_frame.entities_seen);
    info_row("Entities", buf, th::text);
    snprintf(buf, sizeof(buf), "%.0f", g_fps);
    info_row("FPS", buf, th::text);
    info_row("Touch", g_cfg.touch_source == 0 ? "game input" : (touch::available() ? "evdev" : "no evdev"), th::text);
    group_end();
}

void draw_tab(int tab, int col) {
    switch (tab) {
        case 0: tab_ragebot(col); break;
        case 1: tab_legitbot(col); break;
        case 2: tab_trigger(col); break;
        case 3: tab_visuals(col); break;
        case 4: tab_world(col); break;
        case 5: tab_misc(col); break;
        default: tab_configs(col); break;
    }
}

// ---- каркас меню ----
void draw_menu() {
    ImGuiIO& io = ImGui::GetIO();
    float win_w = 690.f * S;
    float win_h = 430.f * S;
    if (win_w > io.DisplaySize.x - 20.f) win_w = io.DisplaySize.x - 20.f;
    if (win_h > io.DisplaySize.y - 20.f) win_h = io.DisplaySize.y - 20.f;
    const float head_h = 44.f * S;
    const float side_w = 142.f * S;

    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - win_w) * 0.5f, (io.DisplaySize.y - win_h) * 0.5f),
                            ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * S);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(th::bg_window));
    ImGui::Begin("##rage", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // шапка
    dl->AddRectFilled(wp, ImVec2(wp.x + win_w, wp.y + head_h), th::bg_header, 8.f * S, ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(wp.x, wp.y + head_h), ImVec2(wp.x + win_w, wp.y + head_h), th::panel_line, 1.f);
    dl->AddRectFilled(ImVec2(wp.x + 16.f * S, wp.y + head_h * 0.5f - 7.f * S),
                      ImVec2(wp.x + 19.5f * S, wp.y + head_h * 0.5f + 7.f * S), th::accent, 2.f * S);
    dl->AddText(ImVec2(wp.x + 30.f * S, wp.y + head_h * 0.5f - ImGui::GetFontSize() * 0.5f), th::text, "RAGEMOD");
    const char* sub = "standoff 2 1.0.0";
    dl->AddText(ImVec2(wp.x + 30.f * S + ImGui::CalcTextSize("RAGEMOD").x + 14.f * S,
                       wp.y + head_h * 0.5f - ImGui::GetFontSize() * 0.5f),
                th::text_mute, sub);

    // Save / Global — как на референсе
    {
        const char* save = "Save";
        ImVec2 ss = ImGui::CalcTextSize(save);
        float bw = ss.x + 22.f * S, bh = 22.f * S;
        ImGui::SetCursorPos(ImVec2(win_w - 52.f * S - bw - 96.f * S, head_h * 0.5f - bh * 0.5f));
        ImGui::PushID("save");
        ImGui::InvisibleButton("##s", ImVec2(bw, bh));
        bool hv = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) config::save();
        ImVec2 r0 = ImGui::GetItemRectMin();
        dl->AddRectFilled(r0, ImVec2(r0.x + bw, r0.y + bh), hv ? th::accent : th::control, 4.f * S);
        dl->AddText(ImVec2(r0.x + (bw - ss.x) * 0.5f, r0.y + (bh - ss.y) * 0.5f), hv ? IM_COL32_WHITE : th::text,
                    save);
        ImGui::PopID();

        const char* cfgname = "Global";
        ImVec2 cs = ImGui::CalcTextSize(cfgname);
        float cwid = cs.x + 26.f * S;
        ImGui::SetCursorPos(ImVec2(win_w - 52.f * S - cwid, head_h * 0.5f - bh * 0.5f));
        ImGui::PushID("cfg");
        ImGui::InvisibleButton("##c", ImVec2(cwid, bh));
        bool hv2 = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_tab = kTabCount - 1;
        ImVec2 c0 = ImGui::GetItemRectMin();
        dl->AddRectFilled(c0, ImVec2(c0.x + cwid, c0.y + bh), th::control, 4.f * S);
        dl->AddText(ImVec2(c0.x + 9.f * S, c0.y + (bh - cs.y) * 0.5f), hv2 ? th::accent : th::text_dim, cfgname);
        ImVec2 tri(c0.x + cwid - 9.f * S, c0.y + bh * 0.5f);
        dl->AddTriangleFilled(ImVec2(tri.x - 3.5f * S, tri.y - 1.5f * S), ImVec2(tri.x + 3.5f * S, tri.y - 1.5f * S),
                              ImVec2(tri.x, tri.y + 2.5f * S), th::text_mute);
        ImGui::PopID();
    }

    ImGui::SetCursorPos(ImVec2(win_w - 34.f * S, head_h * 0.5f - 11.f * S));
    ImGui::PushID("close");
    ImGui::InvisibleButton("##x", ImVec2(22.f * S, 22.f * S));
    bool close_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) g_menu_open = false;
    ImVec2 cc = ImGui::GetItemRectMin();
    ImVec2 c2(cc.x + 11.f * S, cc.y + 11.f * S);
    float cr = 5.f * S;
    ImU32 xcol = close_hov ? th::accent : th::text_dim;
    dl->AddLine(ImVec2(c2.x - cr, c2.y - cr), ImVec2(c2.x + cr, c2.y + cr), xcol, 1.6f * S);
    dl->AddLine(ImVec2(c2.x - cr, c2.y + cr), ImVec2(c2.x + cr, c2.y - cr), xcol, 1.6f * S);
    ImGui::PopID();

    // сайдбар
    dl->AddRectFilled(ImVec2(wp.x, wp.y + head_h), ImVec2(wp.x + side_w, wp.y + win_h), th::bg_side, 8.f * S,
                      ImDrawFlags_RoundCornersBottomLeft);
    dl->AddLine(ImVec2(wp.x + side_w, wp.y + head_h), ImVec2(wp.x + side_w, wp.y + win_h), th::panel_line, 1.f);

    const float item_h = 34.f * S;
    for (int i = 0; i < kTabCount; i++) {
        float extra = (i > 2 ? 9.f * S : 0.f) + (i > 4 ? 9.f * S : 0.f);
        ImGui::SetCursorPos(ImVec2(0.f, head_h + 10.f * S + (float)i * item_h + extra));
        ImGui::PushID(i);
        ImGui::InvisibleButton("##tab", ImVec2(side_w, item_h));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_tab = i;
        bool on = g_tab == i;
        ImVec2 r0 = ImGui::GetItemRectMin();
        if (on) {
            dl->AddRectFilled(r0, ImVec2(r0.x + side_w, r0.y + item_h), th::accent_bg);
            dl->AddRectFilled(r0, ImVec2(r0.x + 2.5f * S, r0.y + item_h), th::accent);
        }
        ImU32 col = on ? th::accent : (hov ? th::text : th::text_dim);
        draw_icon(dl, ImVec2(r0.x + 26.f * S, r0.y + item_h * 0.5f), 7.f * S, kTabs[i].icon, col);
        dl->AddText(ImVec2(r0.x + 44.f * S, r0.y + item_h * 0.5f - ImGui::GetFontSize() * 0.5f), col, kTabs[i].name);
        if (i == 2 || i == 4) {
            float ly = r0.y + item_h + 4.f * S;
            dl->AddLine(ImVec2(r0.x + 16.f * S, ly), ImVec2(r0.x + side_w - 16.f * S, ly), th::panel_line, 1.f);
        }
        ImGui::PopID();
    }

    // контент: две колонки
    const float gap = 13.f * S;
    const float cw = (win_w - side_w - gap * 3.f) * 0.5f;
    const float ch = win_h - head_h - gap * 2.f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    for (int c = 0; c < 2; c++) {
        ImGui::SetCursorPos(ImVec2(side_w + gap + (float)c * (cw + gap), head_h + gap));
        ImGui::BeginChild(c == 0 ? "##c0" : "##c1", ImVec2(cw, ch), 0, ImGuiWindowFlags_NoBackground);
        draw_tab(g_tab, c);
        ImGui::EndChild();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ---- ватермарка, она же кнопка меню ----
void draw_watermark() {
    if (!g_cfg.watermark) return;
    ImGuiIO& io = ImGui::GetIO();

    char line[160];
    time_t raw = time(nullptr);
    struct tm tmv{};
    localtime_r(&raw, &tmv);
    snprintf(line, sizeof(line), "RAGEMOD   |   so2 1.0.0   |   %d fps   |   %d plr   |   %02d:%02d",
             (int)(g_fps + 0.5f), g_frame.count, tmv.tm_hour, tmv.tm_min);

    ImVec2 ts = ImGui::CalcTextSize(line);
    float pad = 11.f * S;
    float w = ts.x + pad * 2.f;
    float h = ts.y + pad;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - w - 14.f * S, 14.f * S), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##wm", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground);

    ImVec2 p = ImGui::GetWindowPos();
    ImGui::InvisibleButton("##wmb", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) g_menu_open = !g_menu_open;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x + 2.f * S, p.y + 3.f * S), ImVec2(p.x + w + 3.f * S, p.y + h + 4.f * S), th::shadow,
                      5.f * S);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), th::bg_header, 5.f * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), hov ? th::accent : th::panel_line, 5.f * S, 0, 1.f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 2.f * S), th::accent, 5.f * S, ImDrawFlags_RoundCornersTop);
    dl->AddText(ImVec2(p.x + pad, p.y + (h - ts.y) * 0.5f + 1.f * S), th::text, line);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---- ESP ----
void draw_box(ImDrawList* dl, const game::PlayerView& p, float sx, float sy, ImU32 c) {
    float h = (p.feet.y - p.top.y) * sy;
    float w = p.box_w * sx;
    if (h < 4.f || w < 2.f) return;
    float cx = p.feet.x * sx;
    float top = p.top.y * sy;
    ImVec2 a(cx - w * 0.5f, top), b(cx + w * 0.5f, top + h);
    dl->AddRect(ImVec2(a.x - 1.f, a.y - 1.f), ImVec2(b.x + 1.f, b.y + 1.f), IM_COL32(0, 0, 0, 160), 0.f, 0, 3.f);
    dl->AddRect(a, b, c, 0.f, 0, 1.4f);
}

void draw_bones(ImDrawList* dl, const game::PlayerView& p, float sx, float sy, ImU32 c) {
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
        dl->AddLine(ImVec2(a->screen.x * sx, a->screen.y * sy), ImVec2(b->screen.x * sx, b->screen.y * sy), c, 1.3f);
    }
    if (const game::BoneView* head = find((int)off::Bone::Head)) {
        float r = (p.feet.y - p.top.y) * sy * 0.07f;
        if (r > 1.5f) dl->AddCircle(ImVec2(head->screen.x * sx, head->screen.y * sy), r, c, 0, 1.3f);
    }
}

void draw_health(ImDrawList* dl, const game::PlayerView& p, float sx, float sy) {
    float h = (p.feet.y - p.top.y) * sy;
    float w = p.box_w * sx;
    if (h < 6.f) return;
    float x = p.feet.x * sx - w * 0.5f - 5.f;
    float top = p.top.y * sy;
    float frac = (float)p.hp / (float)p.maxhp;
    frac = frac < 0.f ? 0.f : (frac > 1.f ? 1.f : frac);
    dl->AddRectFilled(ImVec2(x - 3.f, top - 1.f), ImVec2(x, top + h + 1.f), IM_COL32(0, 0, 0, 170));
    dl->AddRectFilled(ImVec2(x - 3.f, top + h * (1.f - frac)), ImVec2(x, top + h),
                      IM_COL32((int)(255 * (1.f - frac)), (int)(200 * frac + 40), 60, 255));
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

        ImU32 c = p.enemy ? (p.visible ? IM_COL32(240, 70, 70, 245) : IM_COL32(190, 90, 90, 200))
                          : IM_COL32(61, 124, 253, 230);

        if (g_cfg.esp_box) draw_box(dl, p, sx, sy, c);
        if (g_cfg.esp_bones) draw_bones(dl, p, sx, sy, c);
        if (g_cfg.esp_health) draw_health(dl, p, sx, sy);
        if (g_cfg.esp_snaplines) {
            dl->AddLine(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y), ImVec2(p.feet.x * sx, p.feet.y * sy), c,
                        1.1f);
        }
        if (g_cfg.esp_dist) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%.0fm  %d", p.dist, p.hp);
            ImVec2 at(p.feet.x * sx, p.feet.y * sy + 2.f);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            at.x -= ts.x * 0.5f;
            dl->AddText(ImVec2(at.x + 1.f, at.y + 1.f), IM_COL32(0, 0, 0, 200), buf);
            dl->AddText(at, th::text, buf);
        }
    }

    if (g_cfg.draw_fov) {
        float r = io.DisplaySize.y * 0.5f * std::tan(g_cfg.aim_fov * 0.0174533f) /
                  std::tan(g_frame.fov * 0.5f * 0.0174533f);
        dl->AddCircle(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), r, IM_COL32(255, 255, 255, 60), 64,
                      1.2f);
    }
    if (g_frame.has_target && (g_cfg.aim_silent || g_cfg.aim_visible)) {
        dl->AddCircleFilled(ImVec2(g_frame.target_screen.x * sx, g_frame.target_screen.y * sy), 3.5f, th::accent);
    }
}

void draw_debug_line() {
    if (!g_cfg.draw_debug) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    char buf[192];
    snprintf(buf, sizeof(buf), "hooks %d/7 | ent %d | plr %d | cam %dx%d | fov %.0f | %.0f fps",
             mods::installed_count(), g_frame.entities_seen, g_frame.count, g_frame.screen_w, g_frame.screen_h,
             g_frame.fov, g_fps);
    dl->AddText(ImVec2(13.f * S, 13.f * S), IM_COL32(0, 0, 0, 200), buf);
    dl->AddText(ImVec2(12.f * S, 12.f * S), th::accent, buf);
}

// ---- инициализация ----
void apply_style() {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    st.WindowRounding = 8.f * S;
    st.ChildRounding = 6.f * S;
    st.FrameRounding = 4.f * S;
    st.PopupRounding = 6.f * S;
    st.WindowBorderSize = 0.f;
    st.FrameBorderSize = 0.f;
    st.PopupBorderSize = 0.f;
    st.ScrollbarSize = 8.f * S;
    st.ScrollbarRounding = 4.f * S;
    st.ItemSpacing = ImVec2(6.f * S, 5.f * S);
    st.FramePadding = ImVec2(8.f * S, 5.f * S);
    st.WindowPadding = ImVec2(10.f * S, 10.f * S);
    st.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(th::bg_window);
    st.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(th::panel);
    st.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(th::text);
    st.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(th::text_mute);
    st.Colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(th::control);
    st.Colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(th::panel_line);
    st.Colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(th::panel_line);
    st.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(th::control);
    st.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(th::accent);
    st.Colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(th::accent);
    st.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(th::accent_bg);
    st.Colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(th::accent_bg);
    st.Colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(th::accent);
    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(th::control);
    st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(th::accent);
}

void rebuild_font() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x045F, 0x2010, 0x2027, 0};
    io.Fonts->AddFontFromMemoryTTF((void*)kFontRoboto, (int)kFontRoboto_size, 15.f * S, &cfg, ranges);
    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

float wanted_scale(float display_h) {
    float s = (display_h / 700.f) * (g_cfg.menu_scale > 0.f ? g_cfg.menu_scale : 1.f);
    if (s < 0.7f) s = 0.7f;
    if (s > 3.5f) s = 3.5f;
    return s;
}

void init_imgui(float display_h) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui_ImplOpenGL3_Init("#version 300 es");
    S = wanted_scale(display_h);
    apply_style();
    rebuild_font();
    g_imgui_ready = true;
    LOGI("ui: готов, scale %.2f", S);
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
                break;
            case touch::Event::Move:
                io.AddMousePosEvent(e.x, e.y);
                break;
            case touch::Event::Up:
                io.AddMousePosEvent(e.x, e.y);
                io.AddMouseButtonEvent(0, false);
                break;
        }
    }
}

EGLBoolean hk_swap(EGLDisplay dpy, EGLSurface surf) {
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    float w = (float)vp[2], h = (float)vp[3];
    if (w < 16.f || h < 16.f) return o_swap(dpy, surf);

    if (!g_imgui_ready) init_imgui(h);

    // масштаб крутится из меню — шрифт и стиль пересобираем на лету
    static float applied = -1.f;
    float want = wanted_scale(h);
    if (applied < 0.f || want > applied * 1.02f || want < applied * 0.98f) {
        S = want;
        applied = want;
        apply_style();
        rebuild_font();
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(w, h);
    double t = now_s();
    io.DeltaTime = g_last_time > 0.0 ? (float)(t - g_last_time) : 1.f / 60.f;
    if (io.DeltaTime <= 0.f) io.DeltaTime = 1.f / 60.f;
    g_last_time = t;
    g_fps = g_fps * 0.9f + (1.f / io.DeltaTime) * 0.1f;
    touch::set_screen((int)w, (int)h);

    g_frames.fetch_add(1, std::memory_order_relaxed);
    pump_input();
    game::copy_frame(&g_frame);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    draw_esp();
    draw_debug_line();
    draw_watermark();
    if (g_menu_open) draw_menu();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    g_wants_input.store(g_cfg.menu_swallow && (io.WantCaptureMouse || ImGui::IsAnyItemActive()),
                        std::memory_order_relaxed);

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

#ifdef RM_PREVIEW
// хостовое превью меню: рисуем кадр в ImDrawData, рендерит его tools/preview
void preview_init(float display_h) { init_imgui(display_h); }
void preview_frame(int tab, bool menu, const game::Frame& f) {
    g_frame = f;
    g_fps = 144.f;
    g_tab = tab;
    g_menu_open = menu;
    draw_esp();
    draw_watermark();
    if (menu) draw_menu();
}
#endif

bool menu_open() { return g_menu_open; }

bool wants_input() { return g_wants_input.load(std::memory_order_relaxed); }

unsigned long long frames_drawn() { return g_frames.load(std::memory_order_relaxed); }

} // namespace ui
