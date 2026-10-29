// Хостовое превью меню CS2: гоняем тот же ui.cpp, растеризуем ImDrawData в PPM.
// В dll не входит, нужно чтобы смотреть вёрстку без винды и без игры.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "config.h"
#include "game.h"
#include "imgui.h"
#include "ui.h"

// ---- заглушки игровой части ----
Config g_cfg;

namespace config {
const char* path() { return "C:\\Users\\user\\AppData\\Roaming\\ragemod_cs2.json"; }
bool save() { return true; }
bool load() { return true; }
bool bind_active(Bind&, bool base) { return base; }
const char* key_name(int vk) {
    switch (vk) {
        case 0: return "always";
        case 0x02: return "mouse2";
        case 0x06: return "mouse5";
        case 0x20: return "space";
        case 0x2D: return "insert";
        default: return "key";
    }
}
} // namespace config

namespace game {
Frame g_stub;
const BoneLink kBoneLinks[] = {
    {6, 5}, {5, 4}, {4, 0}, {5, 8}, {8, 9}, {9, 10}, {5, 13}, {13, 14}, {14, 15},
    {0, 22}, {22, 23}, {23, 24}, {0, 25}, {25, 26}, {26, 27},
};
const int kBoneLinkCount = (int)(sizeof(kBoneLinks) / sizeof(kBoneLinks[0]));
const Frame& frame() { return g_stub; }
void tick(int, int) {}
} // namespace game

// рисуем правдоподобный кадр: два врага с костями и события попаданий
static void fake_frame(int W, int H) {
    game::Frame& f = game::g_stub;
    f = game::Frame();
    f.in_game = true;
    f.screen_w = W;
    f.screen_h = H;
    f.fps = 240.f;

    struct Spec { float x, foot, head, hp; const char* name; bool vis; };
    const Spec specs[] = {
        {W * 0.33f, H * 0.70f, H * 0.37f, 74.f, "vtablekewl", true},
        {W * 0.62f, H * 0.62f, H * 0.44f, 34.f, "jasong", false},
    };

    for (const Spec& sp : specs) {
        game::PlayerView p;
        p.pawn = (uintptr_t)&sp;
        p.enemy = true;
        p.visible = sp.vis;
        p.on_screen = true;
        p.hp = (int)sp.hp;
        p.dist = 18.f;
        p.feet = {sp.x, sp.foot};
        p.head = {sp.x, sp.head};
        p.box_w = (sp.foot - sp.head) * 0.45f;
        snprintf(p.name, sizeof(p.name), "%s", sp.name);

        float hgt = sp.foot - sp.head;
        float w2 = p.box_w * 0.5f;
        struct B { int id; float dx, dy; };
        const B bones[] = {
            {6, 0.f, 0.02f},   {5, 0.f, 0.17f},   {4, 0.f, 0.34f},  {0, 0.f, 0.52f},
            {8, -0.42f, 0.2f}, {9, -0.6f, 0.38f}, {10, -0.5f, 0.55f},
            {13, 0.42f, 0.2f}, {14, 0.62f, 0.36f}, {15, 0.52f, 0.54f},
            {22, -0.28f, 0.55f}, {23, -0.3f, 0.76f}, {24, -0.26f, 0.98f},
            {25, 0.28f, 0.55f},  {26, 0.3f, 0.76f},  {27, 0.26f, 0.98f},
        };
        for (const B& b : bones) {
            if (p.bone_count >= game::kMaxBones) break;
            p.bones[p.bone_count].bone = b.id;
            p.bones[p.bone_count].on = true;
            p.bones[p.bone_count].screen = {sp.x + b.dx * w2 * 2.f, sp.head + b.dy * hgt};
            p.bone_count++;
        }
        f.players[f.count++] = p;
    }

    game::HitEvent hit;
    hit.ts = 1;
    hit.damage = 27;
    hit.screen = f.players[0].head;
    snprintf(hit.name, sizeof(hit.name), "vtablekewl");
    f.hits[f.hit_count++] = hit;

    game::HitEvent kill;
    kill.ts = 2;
    kill.damage = 100;
    kill.kill = true;
    kill.screen = f.players[1].head;
    snprintf(kill.name, sizeof(kill.name), "jasong");
    f.hits[f.hit_count++] = kill;
}

// ---- мягкий растеризатор ImDrawData ----
struct Image {
    int w = 0, h = 0;
    std::vector<float> px; // rgba
    void init(int W, int H) {
        w = W;
        h = H;
        px.assign((size_t)w * h * 4, 0.f);
        for (size_t i = 0; i < px.size(); i += 4) { // фон «как игра»: тёмно-серый градиент
            int y = (int)(i / 4) / w;
            float t = (float)y / (float)h;
            px[i + 0] = 0.18f + 0.10f * t;
            px[i + 1] = 0.20f + 0.11f * t;
            px[i + 2] = 0.24f + 0.13f * t;
            px[i + 3] = 1.f;
        }
    }
    void blend(int x, int y, float r, float g, float b, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.f) return;
        size_t i = ((size_t)y * w + x) * 4;
        px[i + 0] = px[i + 0] * (1.f - a) + r * a;
        px[i + 1] = px[i + 1] * (1.f - a) + g * a;
        px[i + 2] = px[i + 2] * (1.f - a) + b * a;
    }
};

struct Atlas {
    const unsigned char* px = nullptr;
    int w = 0, h = 0;
    void sample(float u, float v, float* out) const {
        int x = (int)(u * (float)w);
        int y = (int)(v * (float)h);
        x = x < 0 ? 0 : (x >= w ? w - 1 : x);
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);
        const unsigned char* p = px + ((size_t)y * w + x) * 4;
        out[0] = p[0] / 255.f;
        out[1] = p[1] / 255.f;
        out[2] = p[2] / 255.f;
        out[3] = p[3] / 255.f;
    }
};

void raster(Image& img, const Atlas& atlas, ImDrawData* dd) {
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* cl = dd->CmdLists[n];
        const ImDrawVert* vtx = cl->VtxBuffer.Data;
        const ImDrawIdx* idx = cl->IdxBuffer.Data;
        for (const ImDrawCmd& cmd : cl->CmdBuffer) {
            if (cmd.UserCallback) continue;
            const ImDrawIdx* ii = idx + cmd.IdxOffset;
            for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3) {
                const ImDrawVert& a = vtx[cmd.VtxOffset + ii[i + 0]];
                const ImDrawVert& b = vtx[cmd.VtxOffset + ii[i + 1]];
                const ImDrawVert& c = vtx[cmd.VtxOffset + ii[i + 2]];
                float minx = std::floor(std::fmin(a.pos.x, std::fmin(b.pos.x, c.pos.x)));
                float maxx = std::ceil(std::fmax(a.pos.x, std::fmax(b.pos.x, c.pos.x)));
                float miny = std::floor(std::fmin(a.pos.y, std::fmin(b.pos.y, c.pos.y)));
                float maxy = std::ceil(std::fmax(a.pos.y, std::fmax(b.pos.y, c.pos.y)));
                minx = std::fmax(minx, cmd.ClipRect.x);
                miny = std::fmax(miny, cmd.ClipRect.y);
                maxx = std::fmin(maxx, cmd.ClipRect.z);
                maxy = std::fmin(maxy, cmd.ClipRect.w);
                float area = (b.pos.x - a.pos.x) * (c.pos.y - a.pos.y) - (b.pos.y - a.pos.y) * (c.pos.x - a.pos.x);
                if (std::fabs(area) < 1e-6f) continue;
                for (int y = (int)miny; y <= (int)maxy; y++) {
                    for (int x = (int)minx; x <= (int)maxx; x++) {
                        float px = (float)x + 0.5f, py = (float)y + 0.5f;
                        float w0 = ((b.pos.x - a.pos.x) * (py - a.pos.y) - (b.pos.y - a.pos.y) * (px - a.pos.x)) / area;
                        float w1 = ((c.pos.x - b.pos.x) * (py - b.pos.y) - (c.pos.y - b.pos.y) * (px - b.pos.x)) / area;
                        float w2 = ((a.pos.x - c.pos.x) * (py - c.pos.y) - (a.pos.y - c.pos.y) * (px - c.pos.x)) / area;
                        if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
                        float la = w1, lb = w2, lc = w0; // веса вершин a,b,c
                        float u = a.uv.x * la + b.uv.x * lb + c.uv.x * lc;
                        float v = a.uv.y * la + b.uv.y * lb + c.uv.y * lc;
                        float tex[4];
                        atlas.sample(u, v, tex);
                        auto cf = [](ImU32 col, int sh) { return (float)((col >> sh) & 0xff) / 255.f; };
                        float cr = cf(a.col, 0) * la + cf(b.col, 0) * lb + cf(c.col, 0) * lc;
                        float cg = cf(a.col, 8) * la + cf(b.col, 8) * lb + cf(c.col, 8) * lc;
                        float cb = cf(a.col, 16) * la + cf(b.col, 16) * lb + cf(c.col, 16) * lc;
                        float ca = cf(a.col, 24) * la + cf(b.col, 24) * lb + cf(c.col, 24) * lc;
                        img.blend(x, y, cr * tex[0], cg * tex[1], cb * tex[2], ca * tex[3]);
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    int W = argc > 1 ? atoi(argv[1]) : 1920;
    int H = argc > 2 ? atoi(argv[2]) : 1080;
    int tab = argc > 3 ? atoi(argv[3]) : 0;
    const char* out = argc > 4 ? argv[4] : "preview.ppm";

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.DeltaTime = 1.f / 60.f;
    ui::init_style((float)H);

    unsigned char* pixels = nullptr;
    int aw = 0, ah = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &aw, &ah);
    io.Fonts->SetTexID((ImTextureID)1);
    Atlas atlas{pixels, aw, ah};

    // tab < 0 — показываем только худ, меню закрыто
    bool hud_only = tab < 0;
    g_cfg.aim = true;
    g_cfg.aim_bind.key = 0x02;
    g_cfg.trigger = true;
    g_cfg.trigger_bind.key = 0x06;
    g_cfg.trigger_bind.mode = 1;
    g_cfg.trigger_bind.toggled = true;
    g_cfg.bhop = true;
    g_cfg.bhop_bind.key = 0x20;
    fake_frame(W, H);

    if (!hud_only) {
        ui::toggle_menu();
        ui::preview_set_tab(tab);
        if (tab == 1) ui::preview_wait_bind(&g_cfg.trigger_bind);
    }
    for (int i = 0; i < 2; i++) {            // второй кадр: hover/active устаканились
        io.DeltaTime = i == 0 ? 1.f / 60.f : 0.18f;   // чуть состарим эффекты, чтобы не были в нуле
        ImGui::NewFrame();
        ui::frame(W, H, 240.f);
        ImGui::Render();
    }

    Image img;
    img.init(W, H);
    raster(img, atlas, ImGui::GetDrawData());

    FILE* fp = fopen(out, "wb");
    fprintf(fp, "P6\n%d %d\n255\n", W, H);
    for (size_t i = 0; i < img.px.size(); i += 4) {
        unsigned char rgb[3] = {(unsigned char)(img.px[i] * 255.f), (unsigned char)(img.px[i + 1] * 255.f),
                                (unsigned char)(img.px[i + 2] * 255.f)};
        fwrite(rgb, 1, 3, fp);
    }
    fclose(fp);
    printf("%s %dx%d\n", out, W, H);
    return 0;
}
