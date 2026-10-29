#pragma once
// Typed access to the (non-obfuscated) UnityEngine surface. All calls must happen on the Unity main thread.
#include <string>
#include <vector>

#include "common.h"
#include "il2cpp.h"

namespace unity {

using il2::Fn;

// ---- UnityEngine.Object / Component / GameObject / Transform ---------------
inline Fn<bool(void*)> Object_op_Implicit;
inline Fn<void*(void*)> Object_get_name;
inline Fn<int(void*)> Object_GetInstanceID;
inline Fn<void*(void*)> Component_get_transform;
inline Fn<void*(void*)> Component_get_gameObject;
inline Fn<void*(void*, void*, bool)> Component_GetComponentsInChildren;   // (this, Type, includeInactive)
inline Fn<bool(void*)> GameObject_get_activeInHierarchy;
inline Fn<void(void*, bool)> GameObject_SetActive;
inline Fn<void*(void*, void*)> GameObject_GetComponent;       // (this, Type)
inline Fn<void*(void*, void*)> GameObject_AddComponent;       // (this, Type)
inline Fn<Vec3(void*)> Transform_get_position;
inline Fn<Vec3(void*)> Transform_get_forward;
inline Fn<void*(void*)> Transform_get_parent;
inline Fn<void*(void*)> Transform_get_root;

// ---- Camera / Screen / Time -------------------------------------------------
inline Fn<void*()> Camera_get_main;
inline Fn<Vec3(void*, Vec3)> Camera_WorldToScreenPoint;
inline Fn<float(void*)> Camera_get_fieldOfView;
inline Fn<int(void*)> Camera_get_pixelWidth;
inline Fn<int(void*)> Camera_get_pixelHeight;
inline Fn<int()> Screen_get_width;
inline Fn<int()> Screen_get_height;
inline Fn<float()> Time_get_time;
inline Fn<int()> Time_get_frameCount;

// ---- rendering --------------------------------------------------------------
inline Fn<bool(void*)> Renderer_get_enabled;
inline Fn<void(void*, bool)> Renderer_set_enabled;
inline Fn<void*(void*)> Renderer_get_sharedMaterials;
inline Fn<void(void*, void*)> Renderer_set_sharedMaterials;
inline Fn<Bounds(void*)> Renderer_get_bounds;
inline Fn<void*(void*)> Shader_Find;                             // static: (string)
inline Fn<void(void*, void*)> Material_ctor;                     // (this, Shader)
inline Fn<void(void*, void*, Color)> Material_SetColor;         // (this, name, color)
inline Fn<void(void*, void*, int)> Material_SetInt;
inline Fn<void(void*, void*, float)> Material_SetFloat;
inline Fn<void(void*, int)> Material_set_renderQueue;
inline Fn<void(void*, float)> CanvasGroup_set_alpha;
inline Fn<void*(void*, int)> Animator_GetBoneTransform;

// ---- physics ------------------------------------------------------------------
inline Fn<bool(Vec3, Vec3, void*, float, int, int)> Physics_Raycast;  // origin, dir, RaycastHit*, maxDist, mask, QueryTriggerInteraction
inline Fn<void*(void*)> RaycastHit_get_collider;                  // (RaycastHit*)

// ---- classes we need often -----------------------------------------------------
inline il2::Klass* K_Object = nullptr;
inline il2::Klass* K_Material = nullptr;
inline il2::Klass* K_Renderer = nullptr;
inline il2::Klass* K_CanvasGroup = nullptr;

bool init();   // resolves the above; false only when something critical is missing (see il2::stat())

// ---- helpers -----------------------------------------------------------------
std::vector<void*> findAll(il2::Klass* k);             // raw pointers, valid for the current call stack only
bool alive(void* unityObj);
std::string nameOf(void* unityObj);
void* shaderFind(const char* name);
void* newMaterial(void* shader);
void matColor(void* mat, const char* prop, Color c);
void matInt(void* mat, const char* prop, int v);
void* getOrAddCanvasGroup(void* gameObject);
Vec3 w2s(void* cam, Vec3 world, bool* inFront);          // pixels, origin bottom-left (Unity convention)

}  // namespace unity
