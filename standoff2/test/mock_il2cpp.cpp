// Host-side stand-in for libil2cpp.so + the UnityEngine surface the mod uses. It lets the real native game layer
// (il2cpp.cpp / unity.cpp / game.cpp / visuals.cpp / aim.cpp / bridge.cpp) run end to end on a PC against a scripted scene.
// It models semantics that matter for correctness (camera projection, transforms, colliders, instance ids), nothing more.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../native/src/common.h"

extern "C" {
struct MClass;
struct MMethod {
    void* fn;            // methodPointer must be the first member (il2cpp.h::fnPtr reads it)
    void* virt;
    void* inv;
    const char* name;
    MClass* k;
    std::vector<std::string> ptypes;
    int argc;
};
struct MClass {
    std::string ns, name;
    MClass* parent = nullptr;
    std::vector<MMethod*> methods;
    std::map<std::string, int> fields;
    int fieldDummy[1];
};
}

namespace {

constexpr int kObjSize = 1024;
constexpr int OFF_TF = 0x10, OFF_GO = 0x18, OFF_NAME = 0x1C0, OFF_DEAD = 0x1E8, OFF_ID = 0x1F0;

std::map<std::string, MClass*> g_classes;
std::vector<unsigned char*> g_objects;
int g_nextId = 1000;
int g_frameCount = 1;
float g_time = 0;
unsigned char* g_cam = nullptr;
unsigned char* g_shader = nullptr;
std::vector<void*> g_gc(1, nullptr);
float g_wallZ = 1e9f;
unsigned char* g_wall = nullptr;

MClass* K(const char* ns, const char* name) {
    auto it = g_classes.find(std::string(ns) + "." + name);
    return it == g_classes.end() ? nullptr : it->second;
}
MClass* defClass(const char* ns, const char* name, const char* parentNs = nullptr, const char* parent = nullptr) {
    auto* c = new MClass();
    c->ns = ns; c->name = name;
    if (parent) c->parent = K(parentNs, parent);
    g_classes[std::string(ns) + "." + name] = c;
    return c;
}
void addMethod(MClass* c, const char* name, void* fn, std::vector<std::string> ptypes) {
    auto* m = new MMethod{fn, fn, nullptr, name, c, ptypes, static_cast<int>(ptypes.size())};
    c->methods.push_back(m);
}

template <class T> T& at(void* o, int off) { return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + off); }

unsigned char* newObj(MClass* k, const char* name = "") {
    auto* o = static_cast<unsigned char*>(calloc(1, kObjSize));
    at<MClass*>(o, 0) = k;
    at<int>(o, OFF_ID) = g_nextId++;
    strncpy(reinterpret_cast<char*>(o + OFF_NAME), name, 31);
    g_objects.push_back(o);
    return o;
}
bool isA(MClass* k, MClass* base) { for (; k; k = k->parent) if (k == base) return true; return false; }
bool isDead(void* o) { return at<unsigned char>(o, OFF_DEAD) != 0; }

void* mkString(const char* s) {
    size_t n = strlen(s);
    auto* o = static_cast<unsigned char*>(calloc(1, 0x14 + (n + 1) * 2));
    at<int32_t>(o, 0x10) = static_cast<int32_t>(n);
    for (size_t i = 0; i < n; i++) at<uint16_t>(o, 0x14 + i * 2) = static_cast<uint16_t>(s[i]);
    return o;
}
std::string fromString(void* s) {
    if (!s) return "";
    int n = at<int32_t>(s, 0x10);
    std::string r;
    for (int i = 0; i < n; i++) r += static_cast<char>(at<uint16_t>(s, 0x14 + i * 2));
    return r;
}
void* mkArray(size_t n) {
    auto* a = static_cast<unsigned char*>(calloc(1, 0x20 + (n + 1) * 8));
    at<size_t>(a, 0x18) = n;
    return a;
}

// ---- camera math (Unity: left-handed, x right, y up, z forward) ----
struct CamBasis { Vec3 right, up, fwd; };
CamBasis basis() {
    float yaw = at<float>(g_cam, 0x30), pitch = at<float>(g_cam, 0x34);
    Vec3 f{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    Vec3 r{std::cos(yaw), 0, -std::sin(yaw)};
    Vec3 u{f.y * r.z - f.z * r.y, f.z * r.x - f.x * r.z, f.x * r.y - f.y * r.x};
    return {r, u, f};
}

// camera rotation <-> quaternion: Unity euler order (y * x), positive x pitches down, the mock's pitch is up-positive
Quat qmul(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Vec3 qrot(Quat q, Vec3 v) {
    Quat p = qmul(qmul(q, Quat{v.x, v.y, v.z, 0}), Quat{-q.x, -q.y, -q.z, q.w});
    return {p.x, p.y, p.z};
}
Quat camQuat() {
    float yaw = at<float>(g_cam, 0x30), pitch = at<float>(g_cam, 0x34);
    return qmul(Quat{0, std::sin(yaw * 0.5f), 0, std::cos(yaw * 0.5f)}, Quat{std::sin(-pitch * 0.5f), 0, 0, std::cos(-pitch * 0.5f)});
}
void setCamQuat(Quat q) {
    Vec3 f = qrot(q, Vec3{0, 0, 1});
    at<float>(g_cam, 0x30) = std::atan2(f.x, f.z);
    at<float>(g_cam, 0x34) = std::asin(f.y < -1.f ? -1.f : (f.y > 1.f ? 1.f : f.y));
}
int g_rotWrites = 0;

// ================= Unity methods (ABI: args..., MethodInfo*) =================
bool m_op_Implicit(void* o, void*) { return o && !isDead(o); }
void* m_get_name(void* o, void*) { return mkString(reinterpret_cast<char*>(static_cast<unsigned char*>(o) + OFF_NAME)); }
int m_GetInstanceID(void* o, void*) { return at<int>(o, OFF_ID); }
void* m_get_transform(void* o, void*) { return at<void*>(o, OFF_TF) ? at<void*>(o, OFF_TF) : o; }
void* m_get_gameObject(void* o, void*) { return at<void*>(o, OFF_GO); }
void* m_GetComponentsInChildren(void*, void*, bool, void*) { return mkArray(0); }
bool m_activeInHierarchy(void*, void*) { return true; }
void m_SetActive(void*, bool, void*) {}
void* m_AddComponent(void* go, void* type, void*) {
    MClass* k = at<MClass*>(type, 0x20);
    unsigned char* o = newObj(k);
    at<void*>(o, OFF_GO) = go;
    return o;
}
void* m_GetComponent(void* go, void* type, void*) {
    MClass* k = at<MClass*>(type, 0x20);
    for (unsigned char* o : g_objects) if (at<void*>(o, OFF_GO) == go && at<MClass*>(o, 0) == k) return o;
    return nullptr;
}
Vec3 m_get_position(void* t, void*) { return at<Vec3>(t, 0x20); }
Vec3 m_get_forward(void* t, void*) { return t == g_cam ? basis().fwd : at<Vec3>(t, 0x30); }
Quat m_get_rotation(void* t, void*) { return t == g_cam ? camQuat() : Quat{}; }   // the camera has no parent: local == world
void m_set_rotation(void* t, Quat q, void*) { if (t == g_cam) { setCamQuat(q); g_rotWrites++; } }
void* m_get_parent(void* t, void*) { return at<void*>(t, 0x40); }
void* m_get_root(void* t, void*) { return at<void*>(t, 0x48) ? at<void*>(t, 0x48) : t; }

void* m_Camera_main(void*) { return g_cam; }
Vec3 m_W2S(void* cam, Vec3 p, void*) {
    CamBasis b = basis();
    Vec3 pos = at<Vec3>(cam, 0x20);
    Vec3 rel = p - pos;
    float x = dot(rel, b.right), y = dot(rel, b.up), z = dot(rel, b.fwd);
    float W = at<int>(cam, 0x3C), H = at<int>(cam, 0x40);
    float f = (H * 0.5f) / std::tan(at<float>(cam, 0x38) * 0.5f);
    if (z < 0.0001f) return {W * 0.5f + x * 1e4f, H * 0.5f + y * 1e4f, z};
    return {W * 0.5f + x / z * f, H * 0.5f + y / z * f, z};
}
float m_fov(void* cam, void*) { return at<float>(cam, 0x38) * 180.f / 3.14159265f; }
int m_pixW(void* cam, void*) { return at<int>(cam, 0x3C); }
int m_pixH(void* cam, void*) { return at<int>(cam, 0x40); }
int m_screenW(void*) { return 2400; }
int m_screenH(void*) { return 1080; }
float m_time(void*) { return g_time; }
int m_frameCount(void*) { return g_frameCount; }

bool m_r_get_enabled(void* r, void*) { return at<unsigned char>(r, 0x28) != 0; }
void m_r_set_enabled(void* r, bool v, void*) { at<unsigned char>(r, 0x28) = v; }
void* m_r_get_mats(void* r, void*) {
    void* arr = at<void*>(r, 0x20);
    if (!arr) return mkArray(0);
    size_t n = at<size_t>(arr, 0x18);
    void* out = mkArray(n);
    for (size_t i = 0; i < n; i++) static_cast<void**>(static_cast<void*>(static_cast<unsigned char*>(out) + 0x20))[i] = reinterpret_cast<void**>(static_cast<unsigned char*>(arr) + 0x20)[i];
    return out;
}
void m_r_set_mats(void* r, void* arr, void*) { at<void*>(r, 0x20) = arr; }
Bounds m_r_bounds(void*, void*) { return {}; }
void* m_Shader_Find(void*, void*) { return g_shader; }
void m_Material_ctor(void* m, void* sh, void*) { at<void*>(m, 0x20) = sh; }
void m_Material_SetColor(void* m, void*, Color c, void*) { at<Color>(m, 0x30) = c; }
void m_Material_SetInt(void*, void*, int, void*) {}
void m_Material_SetFloat(void*, void*, float, void*) {}
void m_Material_set_renderQueue(void* m, int q, void*) { at<int>(m, 0x28) = q; }
void m_CG_set_alpha(void* cg, float a, void*) { at<float>(cg, 0x20) = a; }
void* m_GetBoneTransform(void* anim, int id, void*) { return id >= 0 && id < 60 ? at<void*>(anim, 0x20 + 8 * id) : nullptr; }

bool m_Raycast(Vec3 o, Vec3 d, void* hit, float maxDist, int, int, void*) {
    // one vertical wall at z = g_wallZ spanning x in [-1000,1000]
    if (g_wall && d.z > 1e-5f) {
        float t = (g_wallZ - o.z) / d.z;
        if (t > 0 && t < maxDist) { at<void*>(hit, 0) = g_wall; return true; }
    }
    return false;
}
void* m_hit_get_collider(void* hit, void*) { return at<void*>(hit, 0); }

void* m_FindObjectsOfType(void* typeObj, void*) {
    MClass* k = at<MClass*>(typeObj, 0x20);
    std::vector<void*> found;
    for (unsigned char* o : g_objects) if (!isDead(o) && isA(at<MClass*>(o, 0), k)) found.push_back(o);
    void* arr = mkArray(found.size());
    for (size_t i = 0; i < found.size(); i++) reinterpret_cast<void**>(static_cast<unsigned char*>(arr) + 0x20)[i] = found[i];
    return arr;
}

void setup() {
    if (!g_classes.empty()) return;
    MClass* obj = defClass("UnityEngine", "Object");
    MClass* comp = defClass("UnityEngine", "Component", "UnityEngine", "Object");
    MClass* go = defClass("UnityEngine", "GameObject", "UnityEngine", "Object");
    MClass* tr = defClass("UnityEngine", "Transform", "UnityEngine", "Component");
    MClass* beh = defClass("UnityEngine", "Behaviour", "UnityEngine", "Component");
    MClass* cam = defClass("UnityEngine", "Camera", "UnityEngine", "Behaviour");
    MClass* ren = defClass("UnityEngine", "Renderer", "UnityEngine", "Component");
    defClass("UnityEngine", "SkinnedMeshRenderer", "UnityEngine", "Renderer");
    MClass* mat = defClass("UnityEngine", "Material", "UnityEngine", "Object");
    MClass* sh = defClass("UnityEngine", "Shader", "UnityEngine", "Object");
    MClass* an = defClass("UnityEngine", "Animator", "UnityEngine", "Behaviour");
    MClass* ph = defClass("UnityEngine", "Physics");
    MClass* rh = defClass("UnityEngine", "RaycastHit");
    MClass* tm = defClass("UnityEngine", "Time");
    MClass* sc = defClass("UnityEngine", "Screen");
    MClass* cg = defClass("UnityEngine", "CanvasGroup", "UnityEngine", "Behaviour");
    MClass* ty = defClass("System", "Type");
    MClass* cb = defClass("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings", "UnityEngine", "Component");
    cb->fields = {{"_bones", 0x20}, {"<Tool>k__BackingField", 0x28}, {"<Animator>k__BackingField", 0x30}, {"<CharacterRenderer>k__BackingField", 0x38}};
    MClass* zone = defClass("Axlebolt.Standoff.Level.Definitions.Zone.Spawn", "TeamSpawnZone", "UnityEngine", "Component");
    zone->fields = {{"_team", 0x28}};
    MClass* blind = defClass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.BlindEffect", "GrenadeBlindEffectView", "UnityEngine", "Component");
    blind->fields = {{"_flashEffectParent", 0x20}, {"_screenshotParent", 0x28}};
    defClass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades", "GrenadeStunEffectView", "UnityEngine", "Component");
    defClass("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.Smoke", "GrenadeSmokeEffectView", "UnityEngine", "Component");
    defClass("Axlebolt.Standoff.Gameplay.Grenade.GrenadeDetonation.Smoke.Components.SmokeEffectModifier", "SmokeEffectModifiersView", "UnityEngine", "Component");
    (void)ty; (void)beh; (void)ren; (void)sh;

    addMethod(obj, "op_Implicit", (void*)m_op_Implicit, {"UnityEngine.Object"});
    addMethod(obj, "get_name", (void*)m_get_name, {});
    addMethod(obj, "GetInstanceID", (void*)m_GetInstanceID, {});
    addMethod(obj, "FindObjectsOfType", (void*)m_FindObjectsOfType, {"System.Type"});
    addMethod(comp, "get_transform", (void*)m_get_transform, {});
    addMethod(comp, "get_gameObject", (void*)m_get_gameObject, {});
    addMethod(comp, "GetComponentsInChildren", (void*)m_GetComponentsInChildren, {"System.Type", "System.Boolean"});
    addMethod(go, "get_activeInHierarchy", (void*)m_activeInHierarchy, {});
    addMethod(go, "SetActive", (void*)m_SetActive, {"System.Boolean"});
    addMethod(go, "GetComponent", (void*)m_GetComponent, {"System.Type"});
    addMethod(go, "AddComponent", (void*)m_AddComponent, {"System.Type"});
    addMethod(tr, "get_position", (void*)m_get_position, {});
    addMethod(tr, "get_forward", (void*)m_get_forward, {});
    addMethod(tr, "get_rotation", (void*)m_get_rotation, {});
    addMethod(tr, "set_rotation", (void*)m_set_rotation, {"UnityEngine.Quaternion"});
    addMethod(tr, "get_localRotation", (void*)m_get_rotation, {});
    addMethod(tr, "set_localRotation", (void*)m_set_rotation, {"UnityEngine.Quaternion"});
    addMethod(tr, "get_parent", (void*)m_get_parent, {});
    addMethod(tr, "get_root", (void*)m_get_root, {});
    addMethod(cam, "get_main", (void*)m_Camera_main, {});
    addMethod(cam, "WorldToScreenPoint", (void*)m_W2S, {"UnityEngine.Vector3"});
    addMethod(cam, "get_fieldOfView", (void*)m_fov, {});
    addMethod(cam, "get_pixelWidth", (void*)m_pixW, {});
    addMethod(cam, "get_pixelHeight", (void*)m_pixH, {});
    addMethod(sc, "get_width", (void*)m_screenW, {});
    addMethod(sc, "get_height", (void*)m_screenH, {});
    addMethod(tm, "get_time", (void*)m_time, {});
    addMethod(tm, "get_frameCount", (void*)m_frameCount, {});
    addMethod(ren, "get_enabled", (void*)m_r_get_enabled, {});
    addMethod(ren, "set_enabled", (void*)m_r_set_enabled, {"System.Boolean"});
    addMethod(ren, "get_sharedMaterials", (void*)m_r_get_mats, {});
    addMethod(ren, "set_sharedMaterials", (void*)m_r_set_mats, {"UnityEngine.Material[]"});
    addMethod(ren, "get_bounds", (void*)m_r_bounds, {});
    addMethod(sh, "Find", (void*)m_Shader_Find, {"System.String"});
    addMethod(mat, ".ctor", (void*)m_Material_ctor, {"UnityEngine.Shader"});
    addMethod(mat, "SetColor", (void*)m_Material_SetColor, {"System.String", "UnityEngine.Color"});
    addMethod(mat, "SetInt", (void*)m_Material_SetInt, {"System.String", "System.Int32"});
    addMethod(mat, "SetFloat", (void*)m_Material_SetFloat, {"System.String", "System.Single"});
    addMethod(mat, "set_renderQueue", (void*)m_Material_set_renderQueue, {"System.Int32"});
    addMethod(cg, "set_alpha", (void*)m_CG_set_alpha, {"System.Single"});
    addMethod(an, "GetBoneTransform", (void*)m_GetBoneTransform, {"UnityEngine.HumanBodyBones"});
    addMethod(ph, "Raycast", (void*)m_Raycast, {"UnityEngine.Vector3", "UnityEngine.Vector3", "UnityEngine.RaycastHit&", "System.Single", "System.Int32", "UnityEngine.QueryTriggerInteraction"});
    addMethod(rh, "get_collider", (void*)m_hit_get_collider, {});

    g_shader = newObj(sh, "Hidden/Internal-Colored");
    g_cam = newObj(cam, "Main Camera");
    at<Vec3>(g_cam, 0x20) = {0, 1.6f, 0};
    at<float>(g_cam, 0x38) = 60.f * 3.14159265f / 180.f;
    at<int>(g_cam, 0x3C) = 2400;
    at<int>(g_cam, 0x40) = 1080;
}

std::string typeName(const std::string& s) { return s; }

}  // namespace

// ============================== the il2cpp C API ==============================
extern "C" {

void* il2cpp_domain_get() { setup(); return reinterpret_cast<void*>(0x1000); }
void** il2cpp_domain_get_assemblies(void*, size_t* n) {
    static void* asms[12];
    for (int i = 0; i < 12; i++) asms[i] = reinterpret_cast<void*>(0x2000 + i * 16);
    *n = 12;
    return asms;
}
void* il2cpp_assembly_get_image(void* a) { return a; }
const char* il2cpp_image_get_name(void*) { return "mock"; }
void* il2cpp_class_from_name(void* image, const char* ns, const char* name) {
    if (image != reinterpret_cast<void*>(0x2000)) return nullptr;   // everything lives in the first image
    setup();
    return K(ns, name);
}
void* il2cpp_class_get_methods(void* k, void** iter) {
    auto* c = static_cast<MClass*>(k);
    size_t i = reinterpret_cast<size_t>(*iter);
    if (i >= c->methods.size()) return nullptr;
    *iter = reinterpret_cast<void*>(i + 1);
    return c->methods[i];
}
void* il2cpp_class_get_method_from_name(void* k, const char* name, int argc) {
    for (auto* c = static_cast<MClass*>(k); c; c = c->parent)
        for (auto* m : c->methods) if (std::string(m->name) == name && (argc < 0 || m->argc == argc)) return m;
    return nullptr;
}
void* il2cpp_class_get_field_from_name(void* k, const char* name) {
    auto* c = static_cast<MClass*>(k);
    auto it = c->fields.find(name);
    if (it == c->fields.end()) return nullptr;
    return new int(it->second);
}
void* il2cpp_class_get_parent(void* k) { return static_cast<MClass*>(k)->parent; }
void* il2cpp_class_get_type(void* k) { return k; }
bool il2cpp_class_is_subclass_of(void* k, void* base, bool) { return isA(static_cast<MClass*>(k)->parent, static_cast<MClass*>(base)); }
size_t il2cpp_field_get_offset(void* f) { return *static_cast<int*>(f); }
void il2cpp_field_static_get_value(void*, void* out) { *static_cast<void**>(out) = nullptr; }
void* il2cpp_object_get_class(void* o) { return at<MClass*>(o, 0); }
void* il2cpp_type_get_object(const void* t) { auto* o = newObj(K("System", "Type")); at<const void*>(o, 0x20) = t; return o; }
char* il2cpp_type_get_name(const void* t) { return strdup(static_cast<const char*>(t)); }
void* il2cpp_string_new(const char* s) { return mkString(s); }
void* il2cpp_runtime_invoke(void*, void*, void**, void**) { return nullptr; }
void* il2cpp_thread_attach(void*) { return nullptr; }
uint32_t il2cpp_gchandle_new(void* o, bool) { g_gc.push_back(o); return static_cast<uint32_t>(g_gc.size() - 1); }
void* il2cpp_gchandle_get_target(uint32_t h) { return h < g_gc.size() ? g_gc[h] : nullptr; }
void il2cpp_gchandle_free(uint32_t h) { if (h < g_gc.size()) g_gc[h] = nullptr; }
void* il2cpp_array_new(void*, size_t n) { return mkArray(n); }
uint32_t il2cpp_method_get_param_count(const void* m) { return static_cast<const MMethod*>(m)->argc; }
const void* il2cpp_method_get_param(const void* m, uint32_t i) { return static_cast<const MMethod*>(m)->ptypes[i].c_str(); }
const char* il2cpp_method_get_name(const void* m) { return static_cast<const MMethod*>(m)->name; }
void* il2cpp_value_box(void*, void*) { return nullptr; }
void* il2cpp_object_new(void* k) { return newObj(static_cast<MClass*>(k)); }
void il2cpp_free(void* p) { free(p); }

// ============================== scene scripting for tests ==============================
void mock_reset() {
    for (auto* o : g_objects) if (o != g_cam && o != g_shader) at<unsigned char>(o, OFF_DEAD) = 1;
    g_wall = nullptr; g_wallZ = 1e9f;
    at<Vec3>(g_cam, 0x20) = {0, 1.6f, 0};
    at<float>(g_cam, 0x30) = 0; at<float>(g_cam, 0x34) = 0;
    g_frameCount = 1;
}
void mock_next_frame() { g_frameCount++; g_time += 1.f / 60.f; }
void mock_set_camera(float x, float y, float z, float yaw, float pitch) { at<Vec3>(g_cam, 0x20) = {x, y, z}; at<float>(g_cam, 0x30) = yaw; at<float>(g_cam, 0x34) = pitch; }
float mock_cam_yaw() { return at<float>(g_cam, 0x30); }
float mock_cam_pitch() { return at<float>(g_cam, 0x34); }
void mock_cam_rotate(float dyaw, float dpitch) { at<float>(g_cam, 0x30) += dyaw; at<float>(g_cam, 0x34) += dpitch; }
int mock_rot_writes() { return g_rotWrites; }
void mock_set_wall(float z) {
    g_wallZ = z;
    if (!g_wall) { g_wall = newObj(K("UnityEngine", "Component"), "wall"); at<void*>(g_wall, OFF_TF) = g_wall; }
}

// A character = root transform + CharacterBindings + Animator with a simple humanoid + renderer.
int mock_add_char(float x, float y, float z, const char* toolName) {
    setup();
    MClass* kt = K("UnityEngine", "Transform");
    unsigned char* root = newObj(kt, "Player");
    at<Vec3>(root, 0x20) = {x, y, z};
    at<void*>(root, OFF_TF) = root;
    at<void*>(root, 0x48) = root;
    unsigned char* bind = newObj(K("Axlebolt.Standoff.Gameplay.Features.Character", "CharacterBindings"));
    at<void*>(bind, OFF_TF) = root;
    unsigned char* anim = newObj(K("UnityEngine", "Animator"));
    // bone heights above the root (metres) and lateral offsets
    struct B { int id; float dx, dy; } bones[] = {{10, 0, 1.62f}, {9, 0, 1.48f}, {8, 0, 1.30f}, {0, 0, 1.0f}, {13, -0.2f, 1.42f}, {15, -0.3f, 1.2f}, {17, -0.3f, 1.0f},
                                                  {14, 0.2f, 1.42f}, {16, 0.3f, 1.2f}, {18, 0.3f, 1.0f}, {1, -0.1f, 0.95f}, {3, -0.1f, 0.5f}, {5, -0.1f, 0.05f},
                                                  {2, 0.1f, 0.95f}, {4, 0.1f, 0.5f}, {6, 0.1f, 0.05f}};
    for (auto& b : bones) {
        unsigned char* t = newObj(kt, "bone");
        at<Vec3>(t, 0x20) = {x + b.dx, y + b.dy, z};
        at<void*>(t, OFF_TF) = t;
        at<void*>(t, 0x48) = root;
        at<void*>(anim, 0x20 + 8 * b.id) = t;
    }
    at<void*>(bind, 0x30) = anim;
    unsigned char* rend = newObj(K("UnityEngine", "SkinnedMeshRenderer"));
    at<unsigned char>(rend, 0x28) = 1;
    void* mats = mkArray(2);
    unsigned char* orig0 = newObj(K("UnityEngine", "Material"), "orig0");
    unsigned char* orig1 = newObj(K("UnityEngine", "Material"), "orig1");
    reinterpret_cast<void**>(static_cast<unsigned char*>(mats) + 0x20)[0] = orig0;
    reinterpret_cast<void**>(static_cast<unsigned char*>(mats) + 0x20)[1] = orig1;
    at<void*>(rend, 0x20) = mats;
    at<void*>(bind, 0x38) = rend;
    unsigned char* tool = newObj(K("UnityEngine", "GameObject"), toolName);
    at<void*>(bind, 0x28) = tool;
    return at<int>(bind, OFF_ID);
}
void* find_by_id(int id) { for (auto* o : g_objects) if (at<int>(o, OFF_ID) == id) return o; return nullptr; }
void mock_move_char(int id, float x, float y, float z) {
    unsigned char* bind = static_cast<unsigned char*>(find_by_id(id));
    unsigned char* root = at<unsigned char*>(bind, OFF_TF);
    Vec3 old = at<Vec3>(root, 0x20);
    Vec3 d{x - old.x, y - old.y, z - old.z};
    for (auto* o : g_objects) if (at<void*>(o, 0x48) == root || o == root) { Vec3& p = at<Vec3>(o, 0x20); p = p + d; }
}
void mock_kill_obj(int id) { if (void* o = find_by_id(id)) at<unsigned char>(o, OFF_DEAD) = 1; }
void mock_add_zone(float x, float y, float z, int team) {
    setup();
    unsigned char* root = newObj(K("UnityEngine", "Transform"), "zone");
    at<Vec3>(root, 0x20) = {x, y, z};
    at<void*>(root, OFF_TF) = root;
    unsigned char* zn = newObj(K("Axlebolt.Standoff.Level.Definitions.Zone.Spawn", "TeamSpawnZone"));
    at<void*>(zn, OFF_TF) = root;
    at<unsigned char>(zn, 0x28) = static_cast<unsigned char>(team);
}
// returns the material pointer currently in slot 0 of the character's renderer (to check chams), or null
void* mock_char_material(int id, int slot) {
    unsigned char* bind = static_cast<unsigned char*>(find_by_id(id));
    void* rend = at<void*>(bind, 0x38);
    void* mats = at<void*>(rend, 0x20);
    return reinterpret_cast<void**>(static_cast<unsigned char*>(mats) + 0x20)[slot];
}
size_t mock_char_material_count(int id) {
    unsigned char* bind = static_cast<unsigned char*>(find_by_id(id));
    return at<size_t>(at<void*>(at<void*>(bind, 0x38), 0x20), 0x18);
}
float mock_material_color_r(void* m) { return at<Color>(m, 0x30).r; }
int mock_material_queue(void* m) { return at<int>(m, 0x28); }
const char* mock_object_name(void* o) { return reinterpret_cast<const char*>(static_cast<unsigned char*>(o) + OFF_NAME); }

// HUD objects for the world-effect tests
int mock_add_blind_view() {
    setup();
    unsigned char* v = newObj(K("Axlebolt.Standoff.UI.HUD.GameHud.Grenades.BlindEffect", "GrenadeBlindEffectView"));
    unsigned char* go1 = newObj(K("UnityEngine", "GameObject"), "flashParentGO");
    unsigned char* go2 = newObj(K("UnityEngine", "GameObject"), "shotParentGO");
    unsigned char* t1 = newObj(K("UnityEngine", "Transform"), "flashParent");
    unsigned char* t2 = newObj(K("UnityEngine", "Transform"), "shotParent");
    at<void*>(t1, OFF_GO) = go1; at<void*>(t1, OFF_TF) = t1;
    at<void*>(t2, OFF_GO) = go2; at<void*>(t2, OFF_TF) = t2;
    at<void*>(v, 0x20) = t1; at<void*>(v, 0x28) = t2;
    at<void*>(v, OFF_TF) = v;
    return at<int>(v, OFF_ID);
}
float mock_canvasgroup_alpha_for(const char* goName) {
    for (auto* o : g_objects) {
        if (at<MClass*>(o, 0) == K("UnityEngine", "CanvasGroup")) {
            void* go = at<void*>(o, OFF_GO);
            if (go && strcmp(mock_object_name(go), goName) == 0) return at<float>(o, 0x20);
        }
    }
    return -1.f;
}
}  // extern "C"
