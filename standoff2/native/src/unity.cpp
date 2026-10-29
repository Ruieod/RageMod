#include "unity.h"

namespace unity {

bool init() {
    using namespace il2;
    K_Object = klass("UnityEngine", "Object");
    K_Material = klass("UnityEngine", "Material");
    K_Renderer = klass("UnityEngine", "Renderer");
    K_CanvasGroup = klass("UnityEngine", "CanvasGroup");

    BIND(Object_op_Implicit, "UnityEngine", "Object", "op_Implicit", 1);
    BIND(Object_get_name, "UnityEngine", "Object", "get_name", 0);
    BIND(Object_GetInstanceID, "UnityEngine", "Object", "GetInstanceID", 0);
    BIND(Component_get_transform, "UnityEngine", "Component", "get_transform", 0);
    BIND(Component_get_gameObject, "UnityEngine", "Component", "get_gameObject", 0);
    BIND_SIG(Component_GetComponentsInChildren, "UnityEngine", "Component", "GetComponentsInChildren", "System.Type", "System.Boolean");
    BIND(GameObject_get_activeInHierarchy, "UnityEngine", "GameObject", "get_activeInHierarchy", 0);
    BIND(GameObject_SetActive, "UnityEngine", "GameObject", "SetActive", 1);
    BIND_SIG(GameObject_GetComponent, "UnityEngine", "GameObject", "GetComponent", "System.Type");
    BIND_SIG(GameObject_AddComponent, "UnityEngine", "GameObject", "AddComponent", "System.Type");
    BIND(Transform_get_position, "UnityEngine", "Transform", "get_position", 0);
    BIND(Transform_get_forward, "UnityEngine", "Transform", "get_forward", 0);
    BIND(Transform_get_parent, "UnityEngine", "Transform", "get_parent", 0);
    BIND(Transform_get_root, "UnityEngine", "Transform", "get_root", 0);

    BIND(Camera_get_main, "UnityEngine", "Camera", "get_main", 0);
    BIND(Camera_WorldToScreenPoint, "UnityEngine", "Camera", "WorldToScreenPoint", 1);
    BIND(Camera_get_fieldOfView, "UnityEngine", "Camera", "get_fieldOfView", 0);
    BIND(Camera_get_pixelWidth, "UnityEngine", "Camera", "get_pixelWidth", 0);
    BIND(Camera_get_pixelHeight, "UnityEngine", "Camera", "get_pixelHeight", 0);
    BIND(Screen_get_width, "UnityEngine", "Screen", "get_width", 0);
    BIND(Screen_get_height, "UnityEngine", "Screen", "get_height", 0);
    BIND(Time_get_time, "UnityEngine", "Time", "get_time", 0);
    BIND(Time_get_frameCount, "UnityEngine", "Time", "get_frameCount", 0);

    BIND(Renderer_get_enabled, "UnityEngine", "Renderer", "get_enabled", 0);
    BIND(Renderer_set_enabled, "UnityEngine", "Renderer", "set_enabled", 1);
    BIND(Renderer_get_sharedMaterials, "UnityEngine", "Renderer", "get_sharedMaterials", 0);
    BIND(Renderer_set_sharedMaterials, "UnityEngine", "Renderer", "set_sharedMaterials", 1);
    BIND(Renderer_get_bounds, "UnityEngine", "Renderer", "get_bounds", 0);
    BIND(Shader_Find, "UnityEngine", "Shader", "Find", 1);
    BIND_SIG(Material_ctor, "UnityEngine", "Material", ".ctor", "UnityEngine.Shader");
    BIND_SIG(Material_SetColor, "UnityEngine", "Material", "SetColor", "System.String", "UnityEngine.Color");
    BIND_SIG(Material_SetInt, "UnityEngine", "Material", "SetInt", "System.String", "System.Int32");
    BIND_SIG(Material_SetFloat, "UnityEngine", "Material", "SetFloat", "System.String", "System.Single");
    BIND(Material_set_renderQueue, "UnityEngine", "Material", "set_renderQueue", 1);
    BIND(CanvasGroup_set_alpha, "UnityEngine", "CanvasGroup", "set_alpha", 1);
    BIND(Animator_GetBoneTransform, "UnityEngine", "Animator", "GetBoneTransform", 1);

    BIND_SIG(Physics_Raycast, "UnityEngine", "Physics", "Raycast", "UnityEngine.Vector3", "UnityEngine.Vector3", "UnityEngine.RaycastHit&", "System.Single", "System.Int32", "UnityEngine.QueryTriggerInteraction");
    BIND(RaycastHit_get_collider, "UnityEngine", "RaycastHit", "get_collider", 0);

    // critical set: without these nothing works
    return Object_op_Implicit.ok() && Component_get_transform.ok() && Transform_get_position.ok() &&
           Camera_get_main.ok() && Camera_WorldToScreenPoint.ok();
}

static il2::Fn<void*(void*)> g_findObjectsOfType;
static il2::Fn<void*(void*, int)> g_findObjectsByType;
static bool g_findBound = false;

std::vector<void*> findAll(il2::Klass* k) {
    std::vector<void*> out;
    if (!k) return out;
    if (!g_findBound) {
        g_findBound = true;
        if (!g_findObjectsOfType.bind(il2::method("UnityEngine", "Object", "FindObjectsOfType", 1)))
            g_findObjectsByType.bind(il2::method("UnityEngine", "Object", "FindObjectsByType", 2));
    }
    void* type = il2::typeObject(k);
    void* arr = nullptr;
    try {
        if (g_findObjectsOfType.ok()) arr = g_findObjectsOfType(type);
        else if (g_findObjectsByType.ok()) arr = g_findObjectsByType(type, 0);
    } catch (...) { return out; }
    size_t n = il2::arrayLen(arr);
    if (n > 4096) n = 4096;
    void** d = n ? il2::arrayData(arr) : nullptr;
    for (size_t i = 0; i < n; i++) if (d[i]) out.push_back(d[i]);
    return out;
}

bool alive(void* o) {
    if (!o) return false;
    try { return Object_op_Implicit(o); } catch (...) { return false; }
}

std::string nameOf(void* o) {
    if (!o) return std::string();
    try { return il2::toUtf8(Object_get_name(o)); } catch (...) { return std::string(); }
}

void* shaderFind(const char* name) {
    try { return Shader_Find(il2::newString(name)); } catch (...) { return nullptr; }
}

void* newMaterial(void* shader) {
    if (!shader || !K_Material || !Material_ctor.ok()) return nullptr;
    try {
        void* m = il2::newObject(K_Material);
        Material_ctor(m, shader);
        return m;
    } catch (...) { return nullptr; }
}

void matColor(void* mat, const char* prop, Color c) { try { Material_SetColor(mat, il2::newString(prop), c); } catch (...) {} }
void matInt(void* mat, const char* prop, int v) { try { Material_SetInt(mat, il2::newString(prop), v); } catch (...) {} }

void* getOrAddCanvasGroup(void* go) {
    if (!go || !K_CanvasGroup) return nullptr;
    try {
        void* t = il2::typeObject(K_CanvasGroup);
        void* cg = GameObject_GetComponent(go, t);
        if (!cg || !alive(cg)) cg = GameObject_AddComponent(go, t);
        return cg;
    } catch (...) { return nullptr; }
}

Vec3 w2s(void* cam, Vec3 world, bool* inFront) {
    Vec3 p = Camera_WorldToScreenPoint(cam, world);
    if (inFront) *inFront = p.z > 0.01f;
    return p;
}

}  // namespace unity
