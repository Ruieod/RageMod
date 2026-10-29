// Меню один в один с референсом: сайдбар с логотипом и группами, шапка с Save/Global,
// панели-группы, строки «подпись — бинд — тумблер», ватермарка-плашка.
#include "ui.h"

#include <cstdio>
#include <cmath>
#include <cstring>

#include "config.h"
#include "game.h"
#include "imgui.h"
#include "roboto_font.h"

namespace ui {
namespace {

namespace th {
const ImU32 bg_window  = IM_COL32(11, 13, 19, 252);
const ImU32 bg_side    = IM_COL32(9, 10, 15, 255);
const ImU32 bg_header  = IM_COL32(10, 12, 17, 255);
const ImU32 panel      = IM_COL32(14, 17, 24, 255);
const ImU32 panel_line = IM_COL32(25, 29, 38, 255);
const ImU32 control    = IM_COL32(32, 37, 47, 255);
const ImU32 text       = IM_COL32(215, 219, 226, 255);
const ImU32 text_dim   = IM_COL32(108, 117, 132, 255);
const ImU32 caption    = IM_COL32(78, 86, 102, 255);
const ImU32 accent     = IM_COL32(47, 126, 245, 255);
const ImU32 accent_bg  = IM_COL32(47, 126, 245, 28);
const ImU32 shadow     = IM_COL32(0, 0, 0, 120);
} // namespace th

float S = 1.f;
bool g_open = false;
int g_tab = 0;
Bind* g_binding = nullptr;   // бинд, для которого сейчас ловим клавишу
float g_fps = 0.f;

enum Icon { IcoRage, IcoTrigger, IcoPlayers, IcoWorld, IcoSkins, IcoSettings, IcoConfig };

struct Tab {
    const char* name;
    Icon icon;
    const char* caption;     // подпись группы над пунктом, если начинается новая
};
const Tab kTabs[] = {
    {"Ragebot", IcoRage, "aimbot"},   {"Trigger", IcoTrigger, nullptr},
    {"Players", IcoPlayers, "visuals"}, {"World", IcoWorld, nullptr},
    {"Skins", IcoSkins, nullptr},     {"Settings", IcoSettings, "misc"},
    {"Configs", IcoConfig, nullptr},
};
constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));

void draw_icon(ImDrawList* dl, ImVec2 c, float r, Icon id, ImU32 col) {
    const float t = 1.4f * S;
    switch (id) {
        case IcoRage:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddLine(ImVec2(c.x - r - t * 2.f, c.y), ImVec2(c.x - r * 0.35f, c.y), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.35f, c.y), ImVec2(c.x + r + t * 2.f, c.y), col, t);
            dl->AddLine(ImVec2(c.x, c.y - r - t * 2.f), ImVec2(c.x, c.y - r * 0.35f), col, t);
            dl->AddLine(ImVec2(c.x, c.y + r * 0.35f), ImVec2(c.x, c.y + r + t * 2.f), col, t);
            dl->AddCircleFilled(c, r * 0.22f, col);
            break;
        case IcoTrigger:
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.6f), ImVec2(c.x + r * 0.2f, c.y - r), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.2f, c.y - r), ImVec2(c.x - r * 0.1f, c.y), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.1f, c.y), ImVec2(c.x + r, c.y - r * 0.2f), col, t);
            break;
        case IcoPlayers:
            dl->AddCircle(ImVec2(c.x, c.y - r * 0.45f), r * 0.42f, col, 0, t);
            dl->PathArcTo(ImVec2(c.x, c.y + r * 1.15f), r * 0.95f, 3.3f, 6.1f, 14);
            dl->PathStroke(col, 0, t);
            break;
        case IcoWorld:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, t);
            dl->AddEllipse(c, ImVec2(r * 0.48f, r), col, 0.f, 0, t);
            break;
        case IcoSkins:
            dl->AddRect(ImVec2(c.x - r, c.y - r * 0.7f), ImVec2(c.x + r, c.y + r * 0.7f), col, 2.f * S, 0, t);
            dl->AddLine(ImVec2(c.x - r * 0.3f, c.y - r * 0.7f), ImVec2(c.x - r * 0.3f, c.y + r * 0.7f), col, t);
            break;
        case IcoSettings:
            dl->AddCircle(c, r * 0.45f, col, 0, t);
            for (int i = 0; i < 6; i++) {
                float a = (float)i * 1.0471976f;
                dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.68f, c.y + std::sin(a) * r * 0.68f),
                            ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r), col, t);
            }
            break;
        case IcoConfig:
            dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.f * S, 0, t);
            dl->AddRectFilled(ImVec2(c.x - r * 0.45f, c.y - r), ImVec2(c.x + r * 0.45f, c.y - r * 0.25f), col);
            break;
    }
}

// ---- панель-группа ----
ImDrawListSplitter g_split;
ImVec2 g_group_start;
bool g_in_group = false;

void group_begin(const char* title) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    g_split.Split(dl, 2);
    g_split.SetCurrentChannel(dl, 1);
    g_in_group = true;
    g_group_start = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(0.f, 10.f * S));
    ImGui::Indent(13.f * S);
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddText(p, th::text, title);
    float w = ImGui::GetWindowWidth() - 26.f * S;
    dl->AddLine(ImVec2(p.x, p.y + ImGui::GetFontSize() + 7.f * S),
                ImVec2(p.x + w, p.y + ImGui::GetFontSize() + 7.f * S), th::panel_line, 1.f);
    ImGui::Dummy(ImVec2(0.f, ImGui::GetFontSize() + 13.f * S));
}

void group_end() {
    if (!g_in_group) return;
    ImGui::Unindent(13.f * S);
    ImGui::Dummy(ImVec2(0.f, 10.f * S));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(g_group_start.x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2.f,
              ImGui::GetCursorScreenPos().y);
    g_split.SetCurrentChannel(dl, 0);
    dl->AddRectFilled(g_group_start, p1, th::panel, 5.f * S);
    dl->AddRect(g_group_start, p1, th::panel_line, 5.f * S, 0, 1.f);
    g_split.Merge(dl);
    g_in_group = false;
    ImGui::Dummy(ImVec2(0.f, 11.f * S));
}

float row_h() { return 24.f * S; }
float row_w() { return ImGui::GetContentRegionAvail().x - 13.f * S; }

// кружок бинда слева от тумблера: тап открывает захват клавиши
void bind_dot(ImDrawList* dl, ImVec2 c, Bind* b) {
    if (!b) return;
    float r = 6.5f * S;
    ImGui::SetCursorScreenPos(ImVec2(c.x - r, c.y - r));
    ImGui::PushID(b);
    ImGui::InvisibleButton("##bind", ImVec2(r * 2.f, r * 2.f));
    bool hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) g_binding = (g_binding == b) ? nullptr : b;
    ImGui::PopID();

    bool waiting = g_binding == b;
    ImU32 col = waiting ? th::accent : (b->key ? th::text_dim : th::control);
    dl->AddCircle(c, r, col, 0, 1.4f * S);
    if (b->key) dl->AddCircleFilled(c, r * 0.42f, col);

    const char* label = waiting ? "press key" : (b->key ? config::key_name(b->key) : (hov ? "bind" : nullptr));
    if (label) {
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImU32 lc = waiting ? th::accent : (hov ? th::text : th::caption);
        dl->AddText(ImVec2(c.x - ts.x - 9.f * S, c.y - ts.y * 0.5f), lc, label);
    }
}

bool toggle_row(const char* label, bool* v, Bind* bind = nullptr) {
    ImGui::PushID(label);
    const float h = row_h();
    const float w = row_w();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##r", ImVec2(w - 30.f * S, h));
    bool hovered = ImGui::IsItemHovered();
    bool changed = ImGui::IsItemClicked();
    if (changed) *v = !*v;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    dl->AddText(ImVec2(p.x, p.y + (h - fs) * 0.5f), (*v || hovered) ? th::text : th::text_dim, label);

    ImVec2 knob(p.x + w - 8.f * S, p.y + h * 0.5f);
    float r = 7.f * S;
    dl->AddCircleFilled(knob, r, *v ? th::accent : th::control);
    if (*v) dl->AddCircleFilled(knob, r * 0.42f, IM_COL32(255, 255, 255, 235));
    else dl->AddCircle(knob, r, th::panel_line, 0, 1.2f * S);

    if (bind) {
        ImVec2 saved = ImGui::GetCursorScreenPos();
        bind_dot(dl, ImVec2(p.x + w - 30.f * S, p.y + h * 0.5f), bind);
        ImGui::SetCursorScreenPos(saved);
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
    dl->AddText(ImVec2(p.x + w - ts.x - 12.f * S, p.y + (h - fs) * 0.5f), hovered ? th::accent : th::text, cur);
    ImVec2 c(p.x + w - 5.f * S, p.y + h * 0.5f);
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
    const float h = fs + 16.f * S;
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
    float ty = p.y + fs + 9.f * S;
    float tr = 2.f * S;
    dl->AddRectFilled(ImVec2(p.x, ty - tr), ImVec2(p.x + w, ty + tr), th::control, tr);
    dl->AddRectFilled(ImVec2(p.x, ty - tr), ImVec2(p.x + w * t, ty + tr), th::accent, tr);
    dl->AddCircleFilled(ImVec2(p.x + w * t, ty), 4.5f * S, IM_COL32_WHITE);
    ImGui::PopID();
    return changed;
}

bool slider_int_row(const char* label, int* v, int mn, int mx) {
    float f = (float)*v;
    bool ch = slider_row(label, &f, (float)mn, (float)mx, "%.0f");
    if (ch) *v = (int)(f + 0.5f);
    return ch;
}

void color_row(const char* label, float col[3]) {
    const float h = row_h();
    const float w = row_w();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::InvisibleButton("##c", ImVec2(w, h));
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##cp");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    dl->AddText(ImVec2(p.x, p.y + (h - fs) * 0.5f), th::text_dim, label);
    ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.f));
    dl->AddRectFilled(ImVec2(p.x + w - 26.f * S, p.y + h * 0.5f - 6.f * S),
                      ImVec2(p.x + w, p.y + h * 0.5f + 6.f * S), c, 3.f * S);
    ImGui::SetNextWindowPos(ImVec2(p.x, p.y + h));
    if (ImGui::BeginPopup("##cp")) {
        ImGui::ColorPicker3("##pick", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void hint(const char* t) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(th::caption));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + row_w());
    ImGui::TextUnformatted(t);
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

bool wide_button(const char* label) {
    const float w = row_w();
    const float h = 27.f * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::InvisibleButton("##b", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), hov ? th::accent : th::control, 4.f * S);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), hov ? IM_COL32_WHITE : th::text, label);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.f, 5.f * S));
    return clicked;
}

// ---- вкладки ----
void tab_ragebot(int col) {
    if (col == 0) {
        group_begin("Main");
        toggle_row("Enabled", &g_cfg.aim, &g_cfg.aim_bind);
        toggle_row("Silent (snap on shot)", &g_cfg.aim_silent);
        toggle_row("Auto fire", &g_cfg.aim_auto_fire);
        toggle_row("Visible only", &g_cfg.aim_visible_only);
        group_end();

        group_begin("Accuracy");
        slider_row("Field of view", &g_cfg.aim_fov, 0.5f, 30.f, "%.1f");
        slider_row("Smoothing", &g_cfg.aim_smooth, 1.f, 20.f, "%.1f");
        group_end();
        return;
    }
    group_begin("Selection");
    static const char* bones[] = {"Head", "Neck", "Chest", "Pelvis"};
    static const int ids[] = {6, 5, 4, 0};
    int sel = 0;
    for (int i = 0; i < 4; i++) {
        if (ids[i] == g_cfg.aim_bone) sel = i;
    }
    if (combo_row("Hitbox", &sel, bones, 4)) g_cfg.aim_bone = ids[sel];
    group_end();

    group_begin("Prediction");
    toggle_row("Velocity predict", &g_cfg.aim_predict);
    slider_row("Scale", &g_cfg.aim_predict_scale, 0.f, 3.f, "%.2f");
    hint("Упреждение на тик сети: цель сдвигается по своей скорости.");
    group_end();
}

void tab_trigger(int col) {
    if (col == 0) {
        group_begin("Main");
        toggle_row("Enabled", &g_cfg.trigger, &g_cfg.trigger_bind);
        toggle_row("Enemies only", &g_cfg.trigger_team_check);
        toggle_row("Scoped only", &g_cfg.trigger_scoped_only);
        group_end();
        return;
    }
    group_begin("Timing");
    slider_int_row("Delay (ms)", &g_cfg.trigger_delay_ms, 0, 250);
    hint("Цель под прицелом игра считает сама (m_iIDEntIndex), свой рейкаст не нужен.");
    group_end();

    group_begin("Movement");
    toggle_row("Bunny hop", &g_cfg.bhop, &g_cfg.bhop_bind);
    hint("Прыжок отпускается в воздухе и жмётся в кадр касания земли.");
    group_end();
}

void tab_players(int col) {
    if (col == 0) {
        group_begin("ESP");
        toggle_row("Enabled", &g_cfg.esp);
        toggle_row("Box", &g_cfg.esp_box);
        toggle_row("Skeleton", &g_cfg.esp_skeleton);
        toggle_row("Health bar", &g_cfg.esp_health);
        toggle_row("Name", &g_cfg.esp_name);
        toggle_row("Distance", &g_cfg.esp_dist);
        group_end();
    group_begin("Hit effects");
    toggle_row("Hitmarker", &g_cfg.hitmarker);
    slider_row("Size", &g_cfg.hitmarker_size, 4.f, 20.f, "%.0f");
    toggle_row("Kill effect", &g_cfg.killeffect);
    hint("Урон ловим по просадке HP врага, авторство — по своему счётчику выстрелов.");
    group_end();
        return;
    }
    group_begin("Filters");
    toggle_row("Enemies only", &g_cfg.esp_team_check);
    toggle_row("Visible only", &g_cfg.esp_visible_only);
    slider_row("Max distance", &g_cfg.esp_max_dist, 10.f, 200.f, "%.0f m");
    group_end();

    group_begin("Glow");
    toggle_row("Enabled", &g_cfg.glow);
    static const char* gtypes[] = {"0", "1", "2", "3 (through walls)"};
    combo_row("Type", &g_cfg.glow_type, gtypes, 4);
    color_row("Occluded", g_cfg.glow_color);
    toggle_row("Split visible", &g_cfg.glow_split_visible);
    color_row("Visible", g_cfg.glow_color_visible);
    hint("Пишем CGlowProperty игрока — подсветку рисует сам движок.");
    group_end();

}

void tab_world(int col) {
    if (col == 0) {
        group_begin("Night mode");
        toggle_row("Enabled", &g_cfg.night);
        slider_row("Darkness", &g_cfg.night_alpha, 0.f, 0.95f, "%.2f");
        color_row("Tint", g_cfg.night_tint);
        toggle_row("Keep players bright", &g_cfg.bright_players);
        group_end();
        return;
    }
    group_begin("Overlay");
    toggle_row("FOV circle", &g_cfg.draw_fov);
    toggle_row("Watermark", &g_cfg.watermark);
    group_end();
}

void tab_skins(int col) {
    if (col == 0) {
        group_begin("Skin changer");
        toggle_row("Enabled", &g_cfg.skins);
        slider_int_row("Paint kit", &g_cfg.skin_paintkit, 0, 1200);
        slider_row("Wear", &g_cfg.skin_wear, 0.f, 1.f, "%.3f");
        slider_int_row("Seed", &g_cfg.skin_seed, 0, 1000);
        group_end();
        return;
    }
    group_begin("StatTrak");
    slider_int_row("Kills", &g_cfg.skin_stattrak, -1, 9999);
    hint("-1 — без счётчика. Скин виден только тебе: это клиентские поля fallback.");
    group_end();
}

void tab_settings(int col) {
    if (col == 0) {
        group_begin("Interface");
        slider_row("Menu scale", &g_cfg.menu_scale, 0.7f, 2.f, "%.2f");
        toggle_row("Watermark", &g_cfg.watermark);
        toggle_row("Debug line", &g_cfg.debug);
        group_end();
        return;
    }
    group_begin("Keys");
    info_row("Menu", config::key_name(g_cfg.menu_key), th::text);
    hint("Бинд опции — кружок слева от тумблера: нажми и выбери клавишу.");
    group_end();
}

void tab_configs(int col) {
    const game::Frame& f = game::frame();
    if (col == 0) {
        group_begin("Config");
        if (wide_button("Save")) config::save();
        if (wide_button("Load")) config::load();
        hint(config::path());
        group_end();
        return;
    }
    group_begin("Status");
    char buf[48];
    info_row("Game", f.in_game ? "in game" : "menu", f.in_game ? th::accent : th::text_dim);
    snprintf(buf, sizeof(buf), "%d", f.count);
    info_row("Players", buf, th::text);
    snprintf(buf, sizeof(buf), "%.0f", g_fps);
    info_row("FPS", buf, th::text);
    info_row("Target", f.has_target ? "locked" : "none", f.has_target ? th::accent : th::text_dim);
    group_end();
}

void draw_tab(int tab, int col) {
    switch (tab) {
        case 0: tab_ragebot(col); break;
        case 1: tab_trigger(col); break;
        case 2: tab_players(col); break;
        case 3: tab_world(col); break;
        case 4: tab_skins(col); break;
        case 5: tab_settings(col); break;
        default: tab_configs(col); break;
    }
}

void capture_bind_key() {
    if (!g_binding) return;
    ImGuiIO& io = ImGui::GetIO();

    // мышь: правая/средняя/боковые, левую не трогаем — ей кликают по меню
    static const struct { int imgui_btn; int vk; } kMouse[] = {{1, 0x02}, {2, 0x04}, {3, 0x05}, {4, 0x06}};
    for (const auto& m : kMouse) {
        if (io.MouseClicked[m.imgui_btn]) {
            g_binding->key = m.vk;
            g_binding = nullptr;
            return;
        }
    }

    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
        if (!ImGui::IsKeyPressed((ImGuiKey)k, false)) continue;
        int vk = 0;
        if (k >= ImGuiKey_A && k <= ImGuiKey_Z) vk = 'A' + (k - ImGuiKey_A);
        else if (k >= ImGuiKey_0 && k <= ImGuiKey_9) vk = '0' + (k - ImGuiKey_0);
        else if (k >= ImGuiKey_F1 && k <= ImGuiKey_F12) vk = 0x70 + (k - ImGuiKey_F1);
        else if (k == ImGuiKey_Space) vk = 0x20;
        else if (k == ImGuiKey_LeftShift) vk = 0xA0;
        else if (k == ImGuiKey_LeftCtrl) vk = 0xA2;
        else if (k == ImGuiKey_LeftAlt) vk = 0xA4;
        else if (k == ImGuiKey_Tab) vk = 0x09;
        else if (k == ImGuiKey_Delete) vk = 0;          // снять бинд
        else if (k == ImGuiKey_Escape) { g_binding = nullptr; return; }
        else continue;
        g_binding->key = vk;
        g_binding = nullptr;
        return;
    }
}

void draw_menu() {
    ImGuiIO& io = ImGui::GetIO();
    float win_w = 700.f * S;
    float win_h = 440.f * S;
    if (win_w > io.DisplaySize.x - 20.f) win_w = io.DisplaySize.x - 20.f;
    if (win_h > io.DisplaySize.y - 20.f) win_h = io.DisplaySize.y - 20.f;
    const float head_h = 46.f * S;
    const float side_w = 158.f * S;

    ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - win_w) * 0.5f, (io.DisplaySize.y - win_h) * 0.5f),
                            ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.f * S);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(th::bg_window));
    ImGui::Begin("##rage", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // сайдбар на всю высоту, логотип сверху — как на референсе
    dl->AddRectFilled(wp, ImVec2(wp.x + side_w, wp.y + win_h), th::bg_side, 7.f * S,
                      ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(wp.x + side_w, wp.y), ImVec2(wp.x + side_w, wp.y + win_h), th::panel_line, 1.f);
    dl->AddRectFilled(ImVec2(wp.x + side_w, wp.y), ImVec2(wp.x + win_w, wp.y + head_h), th::bg_header, 7.f * S,
                      ImDrawFlags_RoundCornersTopRight);
    dl->AddLine(ImVec2(wp.x + side_w, wp.y + head_h), ImVec2(wp.x + win_w, wp.y + head_h), th::panel_line, 1.f);
    dl->AddRect(wp, ImVec2(wp.x + win_w, wp.y + win_h), th::panel_line, 7.f * S, 0, 1.f);

    // логотип
    {
        const char* logo = "RAGEMOD";
        float lx = wp.x + 20.f * S;
        float ly = wp.y + head_h * 0.5f - ImGui::GetFontSize() * 0.5f;
        for (int i = 0; logo[i]; i++) {
            char ch[2] = {logo[i], 0};
            dl->AddText(ImVec2(lx, ly), th::text, ch);
            lx += ImGui::CalcTextSize(ch).x + 2.2f * S;   // разрядка, как в оригинале
        }
    }

    // шапка: Save + Global + иконки
    {
        const char* save = "Save";
        ImVec2 ss = ImGui::CalcTextSize(save);
        float bw = ss.x + 26.f * S, bh = 24.f * S;
        ImGui::SetCursorPos(ImVec2(side_w + 16.f * S, head_h * 0.5f - bh * 0.5f));
        ImGui::PushID("hsave");
        ImGui::InvisibleButton("##s", ImVec2(bw, bh));
        bool hv = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) config::save();
        ImVec2 r0 = ImGui::GetItemRectMin();
        dl->AddRectFilled(r0, ImVec2(r0.x + bw, r0.y + bh), hv ? th::accent : th::control, 4.f * S);
        dl->AddText(ImVec2(r0.x + (bw - ss.x) * 0.5f, r0.y + (bh - ss.y) * 0.5f), hv ? IM_COL32_WHITE : th::text,
                    save);
        ImGui::PopID();

        float cw = 150.f * S;
        ImGui::SetCursorPos(ImVec2(side_w + 16.f * S + bw + 10.f * S, head_h * 0.5f - bh * 0.5f));
        ImGui::PushID("hcfg");
        ImGui::InvisibleButton("##g", ImVec2(cw, bh));
        bool hv2 = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_tab = kTabCount - 1;
        ImVec2 c0 = ImGui::GetItemRectMin();
        dl->AddRectFilled(c0, ImVec2(c0.x + cw, c0.y + bh), th::control, 4.f * S);
        dl->AddText(ImVec2(c0.x + 10.f * S, c0.y + (bh - ImGui::GetFontSize()) * 0.5f),
                    hv2 ? th::accent : th::text_dim, "Global");
        ImVec2 tri(c0.x + cw - 11.f * S, c0.y + bh * 0.5f);
        dl->AddTriangleFilled(ImVec2(tri.x - 4.f * S, tri.y - 1.5f * S), ImVec2(tri.x + 4.f * S, tri.y - 1.5f * S),
                              ImVec2(tri.x, tri.y + 2.5f * S), th::text_dim);
        ImGui::PopID();

        // иконки справа
        float ix = win_w - 30.f * S;
        for (int i = 0; i < 2; i++) {
            ImGui::SetCursorPos(ImVec2(ix, head_h * 0.5f - 11.f * S));
            ImGui::PushID(100 + i);
            ImGui::InvisibleButton("##i", ImVec2(22.f * S, 22.f * S));
            bool hv3 = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                if (i == 0) g_open = false;
                else g_tab = 5;
            }
            ImVec2 ic = ImGui::GetItemRectMin();
            ImVec2 cc(ic.x + 11.f * S, ic.y + 11.f * S);
            ImU32 col = hv3 ? th::accent : th::text_dim;
            if (i == 0) {
                float r = 5.f * S;
                dl->AddLine(ImVec2(cc.x - r, cc.y - r), ImVec2(cc.x + r, cc.y + r), col, 1.6f * S);
                dl->AddLine(ImVec2(cc.x - r, cc.y + r), ImVec2(cc.x + r, cc.y - r), col, 1.6f * S);
            } else {
                draw_icon(dl, cc, 7.f * S, IcoSettings, col);
            }
            ImGui::PopID();
            ix -= 28.f * S;
        }
    }

    // пункты сайдбара с подписями групп
    {
        float y = head_h + 14.f * S;
        for (int i = 0; i < kTabCount; i++) {
            if (kTabs[i].caption) {
                ImVec2 cp(wp.x + 20.f * S, wp.y + y);
                dl->AddText(cp, th::caption, kTabs[i].caption);
                y += ImGui::GetFontSize() + 9.f * S;
            }
            ImGui::SetCursorPos(ImVec2(0.f, y));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##tab", ImVec2(side_w, 32.f * S));
            bool hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) g_tab = i;
            bool on = g_tab == i;
            ImVec2 r0 = ImGui::GetItemRectMin();
            if (on) {
                dl->AddRectFilled(r0, ImVec2(r0.x + side_w, r0.y + 32.f * S), th::accent_bg);
                dl->AddRectFilled(r0, ImVec2(r0.x + 2.f * S, r0.y + 32.f * S), th::accent);
            }
            ImU32 col = on ? th::accent : (hov ? th::text : th::text_dim);
            draw_icon(dl, ImVec2(r0.x + 32.f * S, r0.y + 16.f * S), 7.f * S, kTabs[i].icon, col);
            dl->AddText(ImVec2(r0.x + 50.f * S, r0.y + 16.f * S - ImGui::GetFontSize() * 0.5f), col, kTabs[i].name);
            ImGui::PopID();
            y += 32.f * S;
        }

        // чип пользователя внизу сайдбара
        float cy = win_h - 40.f * S;
        dl->AddLine(ImVec2(wp.x + 16.f * S, wp.y + cy - 10.f * S), ImVec2(wp.x + side_w - 16.f * S, wp.y + cy - 10.f * S),
                    th::panel_line, 1.f);
        dl->AddCircleFilled(ImVec2(wp.x + 28.f * S, wp.y + cy + 10.f * S), 9.f * S, th::control);
        dl->AddCircle(ImVec2(wp.x + 28.f * S, wp.y + cy + 10.f * S), 9.f * S, th::accent, 0, 1.2f * S);
        dl->AddText(ImVec2(wp.x + 44.f * S, wp.y + cy + 4.f * S), th::text_dim, "cs2 \xc2\xb7 1.0");
    }

    // контент: две колонки
    const float gap = 14.f * S;
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

    capture_bind_key();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// хитмаркер и килл-эффект: крестик в центре, гаснет сам
void draw_hit_effects() {
    const game::Frame& f = game::frame();
    if (f.last_hit.ts == 0) return;

    static int64_t last_seen = 0;
    static bool beeped = false;
    if (f.last_hit.ts != last_seen) {
        last_seen = f.last_hit.ts;
        beeped = false;
    }

    // возраст события считаем по времени imgui, а не лезем в winapi из ui
    static float age = 10.f;
    static int64_t age_for = 0;
    if (age_for != f.last_hit.ts) {
        age_for = f.last_hit.ts;
        age = 0.f;
    } else {
        age += ImGui::GetIO().DeltaTime;
    }

    const float life = f.last_hit.kill ? 1.1f : 0.35f;
    if (age > life) return;
    float k = 1.f - age / life;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    if (g_cfg.hitmarker) {
        float r = g_cfg.hitmarker_size * S * (f.last_hit.kill ? 1.6f : 1.f);
        float gap = r * 0.45f;
        ImU32 col = f.last_hit.kill ? IM_COL32(255, 60, 60, (int)(255 * k))
                                    : IM_COL32(255, 255, 255, (int)(235 * k));
        float t = 1.8f * S;
        dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x - gap, c.y - gap), col, t);
        dl->AddLine(ImVec2(c.x + gap, c.y + gap), ImVec2(c.x + r, c.y + r), col, t);
        dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x - gap, c.y + gap), col, t);
        dl->AddLine(ImVec2(c.x + gap, c.y - gap), ImVec2(c.x + r, c.y - r), col, t);
    }

    if (g_cfg.killeffect && f.last_hit.kill) {
        const char* txt = "KILL";
        ImVec2 ts = ImGui::CalcTextSize(txt);
        float y = c.y - 44.f * S - (1.f - k) * 18.f * S;
        ImU32 col = IM_COL32(255, 70, 70, (int)(255 * k));
        dl->AddText(ImVec2(c.x - ts.x * 0.5f + 1.f, y + 1.f), IM_COL32(0, 0, 0, (int)(200 * k)), txt);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, y), col, txt);
    } else if (g_cfg.hitmarker && f.last_hit.damage > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", f.last_hit.damage);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float y = c.y - 34.f * S - (1.f - k) * 10.f * S;
        dl->AddText(ImVec2(c.x - ts.x * 0.5f + 1.f, y + 1.f), IM_COL32(0, 0, 0, (int)(190 * k)), buf);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, y), IM_COL32(255, 210, 90, (int)(240 * k)), buf);
    }
    (void)beeped;
}

void draw_watermark() {
    if (!g_cfg.watermark) return;
    ImGuiIO& io = ImGui::GetIO();
    const game::Frame& f = game::frame();

    char line[160];
    snprintf(line, sizeof(line), "RAGEMOD   |   cs2   |   %d fps   |   %d plr   |   %s", (int)(g_fps + 0.5f),
             f.count, f.in_game ? "in game" : "menu");
    ImVec2 ts = ImGui::CalcTextSize(line);
    float pad = 11.f * S;
    float w = ts.x + pad * 2.f, h = ts.y + pad;
    ImVec2 p(io.DisplaySize.x - w - 16.f * S, 16.f * S);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2(p.x + 2.f * S, p.y + 3.f * S), ImVec2(p.x + w + 3.f * S, p.y + h + 4.f * S), th::shadow,
                      5.f * S);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), th::bg_header, 5.f * S);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), th::panel_line, 5.f * S, 0, 1.f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 2.f * S), th::accent, 5.f * S, ImDrawFlags_RoundCornersTop);
    dl->AddText(ImVec2(p.x + pad, p.y + (h - ts.y) * 0.5f + 1.f * S), th::text, line);
}

void draw_esp() {
    const game::Frame& f = game::frame();
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    if (g_cfg.night) {
        ImU32 tint = ImGui::ColorConvertFloat4ToU32(
            ImVec4(g_cfg.night_tint[0], g_cfg.night_tint[1], g_cfg.night_tint[2], g_cfg.night_alpha));
        dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, tint);
    }
    if (!g_cfg.esp) return;

    for (int i = 0; i < f.count; i++) {
        const game::PlayerView& p = f.players[i];
        if (!p.on_screen) continue;
        if (g_cfg.esp_team_check && !p.enemy) continue;
        if (g_cfg.esp_visible_only && !p.visible) continue;
        if (p.dist > g_cfg.esp_max_dist) continue;

        ImU32 c = p.enemy ? (p.visible ? IM_COL32(240, 70, 70, 245) : IM_COL32(190, 90, 90, 205))
                          : IM_COL32(47, 126, 245, 230);
        float h = p.feet.y - p.head.y;
        float w = p.box_w;
        ImVec2 a(p.feet.x - w * 0.5f, p.head.y), b(p.feet.x + w * 0.5f, p.feet.y);

        if (g_cfg.esp_box && h > 4.f) {
            dl->AddRect(ImVec2(a.x - 1.f, a.y - 1.f), ImVec2(b.x + 1.f, b.y + 1.f), IM_COL32(0, 0, 0, 170), 0.f, 0,
                        3.f);
            dl->AddRect(a, b, c, 0.f, 0, 1.4f);
        }
        if (g_cfg.esp_skeleton) {
            auto find = [&](int bone) -> const game::BoneView* {
                for (int k = 0; k < p.bone_count; k++) {
                    if (p.bones[k].bone == bone && p.bones[k].on) return &p.bones[k];
                }
                return nullptr;
            };
            for (int li = 0; li < game::kBoneLinkCount; li++) {
                const game::BoneView* s0 = find(game::kBoneLinks[li].a);
                const game::BoneView* s1 = find(game::kBoneLinks[li].b);
                if (!s0 || !s1) continue;
                dl->AddLine(ImVec2(s0->screen.x, s0->screen.y), ImVec2(s1->screen.x, s1->screen.y), c, 1.3f);
            }
        }
        if (g_cfg.esp_health && h > 6.f) {
            float frac = (float)p.hp / 100.f;
            frac = frac < 0.f ? 0.f : (frac > 1.f ? 1.f : frac);
            float x = a.x - 6.f;
            dl->AddRectFilled(ImVec2(x - 3.f, a.y - 1.f), ImVec2(x, b.y + 1.f), IM_COL32(0, 0, 0, 180));
            dl->AddRectFilled(ImVec2(x - 3.f, a.y + h * (1.f - frac)), ImVec2(x, b.y),
                              IM_COL32((int)(255 * (1.f - frac)), (int)(200 * frac + 40), 60, 255));
        }
        if (g_cfg.esp_name && p.name[0]) {
            ImVec2 ts = ImGui::CalcTextSize(p.name);
            ImVec2 at(p.feet.x - ts.x * 0.5f, p.head.y - ts.y - 3.f);
            dl->AddText(ImVec2(at.x + 1.f, at.y + 1.f), IM_COL32(0, 0, 0, 200), p.name);
            dl->AddText(at, th::text, p.name);
        }
        if (g_cfg.esp_dist) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0fm", p.dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            ImVec2 at(p.feet.x - ts.x * 0.5f, p.feet.y + 2.f);
            dl->AddText(ImVec2(at.x + 1.f, at.y + 1.f), IM_COL32(0, 0, 0, 200), buf);
            dl->AddText(at, c, buf);
        }
    }

    if (g_cfg.draw_fov) {
        float r = io.DisplaySize.y * 0.5f * std::tan(g_cfg.aim_fov * 0.0174533f) / std::tan(45.f * 0.0174533f);
        dl->AddCircle(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), r, IM_COL32(255, 255, 255, 55), 64,
                      1.2f);
    }
    if (f.has_target && g_cfg.aim) {
        dl->AddCircleFilled(ImVec2(f.target_screen.x, f.target_screen.y), 3.5f, th::accent);
    }
}

} // namespace

void init_style(float display_h) {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    S = (display_h / 900.f) * (g_cfg.menu_scale > 0.f ? g_cfg.menu_scale : 1.f);
    if (S < 0.7f) S = 0.7f;
    if (S > 3.f) S = 3.f;
    st.WindowRounding = 7.f * S;
    st.ChildRounding = 5.f * S;
    st.FrameRounding = 4.f * S;
    st.PopupRounding = 5.f * S;
    st.WindowBorderSize = 0.f;
    st.FrameBorderSize = 0.f;
    st.PopupBorderSize = 0.f;
    st.ScrollbarSize = 7.f * S;
    st.ItemSpacing = ImVec2(6.f * S, 5.f * S);
    st.FramePadding = ImVec2(8.f * S, 5.f * S);
    st.WindowPadding = ImVec2(11.f * S, 11.f * S);
    st.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(th::bg_window);
    st.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(th::panel);
    st.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(th::text);
    st.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(th::caption);
    st.Colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(th::control);
    st.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(th::control);
    st.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(th::accent);
    st.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(th::accent_bg);
    st.Colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(th::accent_bg);
    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(th::control);

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
}

void frame(int w, int h, float fps) {
    g_fps = fps;
    draw_esp();
    draw_hit_effects();
    draw_watermark();
    if (g_open) draw_menu();
}

#ifdef RM_PREVIEW
void preview_set_tab(int tab) { g_tab = tab; }
void preview_wait_bind(Bind* b) { g_binding = b; }
#endif

bool menu_open() { return g_open; }
void toggle_menu() { g_open = !g_open; }
bool capturing_input() { return g_open; }

} // namespace ui
