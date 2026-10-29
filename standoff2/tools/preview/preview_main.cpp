// Хостовое превью меню: гоняем тот же ui.cpp, растеризуем ImDrawData в PPM.
// В сборку под Android не входит, нужно только чтобы смотреть вёрстку без телефона.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "game.h"
#include "hook.h"
#include "il2cpp.h"
#include "imgui.h"
#include "mods.h"
#include "touch.h"
#include "ui.h"

// ---- заглушки игровой части ----
namespace game {
void copy_frame(Frame*) {}
void tick(void*) {}
void post_tick(void*) {}
void note_player(void*) {}
AimTarget current_target() { return {}; }
bool trigger_ready() { return false; }
Vec3 aim_point(void*, bool* ok) { if (ok) *ok = false; return {}; }
} // namespace game

namespace mods {
bool install() { return true; }
int installed_count() { return 7; }
int seen_action_count() { return 4; }
const char* seen_action(int i) {
    static const char* n[] = {"Fire", "Aim", "Jump", "Reload"};
    return (i >= 0 && i < 4) ? n[i] : "";
}
} // namespace mods

namespace touch {
void start() {}
void feed(float, float, bool) {}
void set_screen(int, int) {}
bool poll(Event*) { return false; }
bool available() { return true; }
} // namespace touch

namespace il2cpp {
namespace { uintptr_t g_fake = 0x7a1b2c3000ull; }
uintptr_t base() { return g_fake; }
bool wait_for_module(int) { return true; }
bool readable(const void*, size_t) { return false; }
const char* class_name(const void*) { return nullptr; }
bool is_class(const void*, const char*) { return false; }
void* camera_main() { return nullptr; }
Vec3 world_to_screen(void*, const Vec3&) { return {}; }
int camera_pixel_width(void*) { return 0; }
int camera_pixel_height(void*) { return 0; }
float camera_fov(void*) { return 60.f; }
void* component_transform(void*) { return nullptr; }
Vec3 transform_position(void*) { return {}; }
Vec3 transform_forward(void*) { return {}; }
Vec3 transform_euler(void*) { return {}; }
void transform_set_euler(void*, const Vec3&) {}
void transform_set_position(void*, const Vec3&) {}
void* renderer_shared_material(void*) { return nullptr; }
void renderer_set_material(void*, void*) {}
void renderer_set_enabled(void*, bool) {}
int material_render_queue(void*) { return 0; }
void material_set_render_queue(void*, int) {}
bool linecast(const Vec3&, const Vec3&, RaycastHit*, int) { return false; }
Vec3 player_position(void*) { return {}; }
Quat player_rotation(void*) { return {}; }
bool unity_touch(Vec2*, int*) { return false; }
bool unity_mouse(Vec2*, bool*) { return false; }
int string_to_ascii(const void*, char* out, int) { if (out) out[0] = 0; return 0; }
} // namespace il2cpp

namespace hook {
bool inlined(void*, void*, void**) { return true; }
bool got(uintptr_t, const char*, void*, void**) { return true; }
} // namespace hook

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
    int W = argc > 1 ? atoi(argv[1]) : 2340;
    int H = argc > 2 ? atoi(argv[2]) : 1080;
    int tab = argc > 3 ? atoi(argv[3]) : 0;
    const char* out = argc > 4 ? argv[4] : "preview.ppm";

    ui::preview_init((float)H);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.DeltaTime = 1.f / 60.f;

    unsigned char* pixels = nullptr;
    int aw = 0, ah = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &aw, &ah);
    io.Fonts->SetTexID((ImTextureID)1);
    Atlas atlas{pixels, aw, ah};

    game::Frame f;
    f.count = 0;
    f.entities_seen = 14;

    for (int i = 0; i < 2; i++) { // второй кадр: hovered/active state устаканился
        ImGui::NewFrame();
        ui::preview_frame(tab, true, f);
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
