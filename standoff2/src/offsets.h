// Standoff 2 | 1.0.0 | arm64-v8a | com.axlebolt.standoff2 (versionCode 203964)
// RVA — от load bias libunity.so (код il2cpp слит в libunity.so, il2cpp_* экспортов нет).
// Поля — от начала managed-объекта.
// Источник: dump/dump.cs + offsets-source.txt того же дампа.
#pragma once
#include <cstdint>

namespace off {

// методы, которые зовём напрямую
namespace rva {
constexpr uintptr_t Camera_get_main           = 0x6D28F70; // static Camera get_main()
constexpr uintptr_t Camera_WorldToScreenPoint = 0x6D2CFC4; // Vector3 WorldToScreenPoint(Vector3)
constexpr uintptr_t Camera_get_pixelWidth     = 0x6D2A014;
constexpr uintptr_t Camera_get_pixelHeight    = 0x6D2BD48;
constexpr uintptr_t Camera_get_fieldOfView    = 0x6D2640C;
constexpr uintptr_t Component_get_transform   = 0x67E3358;
constexpr uintptr_t Transform_get_position    = 0x67EF458;
constexpr uintptr_t Transform_get_forward     = 0x67F967C;
constexpr uintptr_t Transform_get_eulerAngles = 0x67F7B70;
constexpr uintptr_t Transform_set_eulerAngles = 0x67F4378;
constexpr uintptr_t Physics_Linecast          = 0x905A5A8; // bool(Vector3,Vector3,out RaycastHit,int mask)
constexpr uintptr_t Player_get_position       = 0x9489018; // ake.xdzx() -> Vector3
constexpr uintptr_t Player_get_rotation       = 0x948B514; // ake.ioiu() -> Quaternion
constexpr uintptr_t Transform_set_position    = 0x67F1268;
constexpr uintptr_t Renderer_get_sharedMaterial = 0x7613034;
constexpr uintptr_t Renderer_set_material     = 0x75F2DE8;
constexpr uintptr_t Renderer_set_enabled      = 0x7605850;
constexpr uintptr_t Material_get_renderQueue  = 0x76027CC;
constexpr uintptr_t Material_set_renderQueue  = 0x75FCB04;
}

// точки перехвата
namespace hook_rva {
constexpr uintptr_t CameraPart_tick  = 0xA0903F4; // ios.lxcq()        — кадровый тик камеры локального игрока
constexpr uintptr_t HitCast_fire     = 0x973A818; // jxy.vrsg(ref WeaponContext)
constexpr uintptr_t HitCast_rays     = 0x9739FA4; // jxy.rxjx(ref WeaponContext, Span<Ray>)
constexpr uintptr_t Player_update    = 0x949E8A0; // ake.opmn()        — fallback-сбор игроков
constexpr uintptr_t Input_get_button = 0x8D441BC; // znn.rpcp(string) -> ButtonInput — автострельба
constexpr uintptr_t Recoil_apply     = 0x9731070; // vez.lhgb(ref WeaponContext, bool) — отдача
}

// il2cpp runtime layout (из dump/il2cpp.h этого дампа)
namespace il2 {
constexpr uintptr_t Object_klass   = 0x00;
constexpr uintptr_t Class_name     = 0x10; // const char*
constexpr uintptr_t Class_parent   = 0x48; // image,gc_desc,name,ns,byval(16),this(16),element,cast,declaring,parent
constexpr uintptr_t Array_length   = 0x18; // max_length
constexpr uintptr_t Array_data     = 0x20;
constexpr uintptr_t List_items     = 0x10;
constexpr uintptr_t List_size      = 0x18;
constexpr uintptr_t String_length  = 0x10; // int
constexpr uintptr_t String_chars   = 0x14; // utf-16
}

// поля игровых классов
namespace fld {
// chx — база частей/систем сущности
constexpr uintptr_t chx_owner   = 0x18; // krm  (сущность-владелец)
constexpr uintptr_t chx_world   = 0x20; // ccg  (мир, реально njh)

// mfc — база сетевой сущности
constexpr uintptr_t mfc_netid   = 0x10;
constexpr uintptr_t mfc_world   = 0x18;
constexpr uintptr_t mfc_parts   = 0x28; // rct
constexpr uintptr_t mfc_local   = 0x44; // bool LocalOwned

// ake — игрок (наследник mfc)
constexpr uintptr_t ake_stats   = 0x70; // ylw
constexpr uintptr_t ake_vis     = 0x88; // qdp
constexpr uintptr_t ake_team    = 0x90; // byte

// ylw — характеристики
constexpr uintptr_t ylw_hp        = 0x10; // ushort
constexpr uintptr_t ylw_maxhp     = 0x14; // ushort
constexpr uintptr_t ylw_armor     = 0x16; // ushort
constexpr uintptr_t ylw_maxarmor  = 0x1A; // ushort
constexpr uintptr_t ylw_helmet    = 0x1C; // bool

// qdp — состояние видимости
constexpr uintptr_t qdp_flags0    = 0x10; // bool; назначение флагов не проверено
constexpr uintptr_t qdp_bounds    = 0x18; // Bounds (center 0x18, extents 0x24)

// rct — контейнер частей
constexpr uintptr_t rct_list      = 0x10; // List<eur>

// jgi — модель персонажа
constexpr uintptr_t jgi_gameobject = 0x10;
constexpr uintptr_t jgi_hitboxes   = 0x18; // HitBoxCollider[]
constexpr uintptr_t jgi_transform  = 0x20;
constexpr uintptr_t jgi_animator   = 0x28;
constexpr uintptr_t jgi_bindings   = 0x30; // CharacterBindings

// CharacterBindings : MonoBehaviour (не обфусцирован)
constexpr uintptr_t cb_bones            = 0x20;
constexpr uintptr_t cb_tool             = 0x28; // GameObject
constexpr uintptr_t cb_animator         = 0x30;
constexpr uintptr_t cb_char_renderer    = 0x38; // SkinnedMeshRenderer
constexpr uintptr_t cb_arms_renderer    = 0x40;
constexpr uintptr_t cb_gloves_renderer  = 0x48;
constexpr uintptr_t cb_untouchable_mat  = 0x50; // Material
constexpr uintptr_t cb_outline_mat      = 0x58; // Material

// HitBoxCollider : MonoBehaviour
constexpr uintptr_t hbc_bone   = 0x20; // HitBox.bone (HumanBodyBones, int)
constexpr uintptr_t hbc_loss   = 0x24;
constexpr uintptr_t hbc_col    = 0x28; // Collider
constexpr uintptr_t hbc_owner  = 0x30; // rph (владелец-сущность)

// eir — сетевые углы (две Quaternion, подтверждены дизасмом сеттеров)
constexpr uintptr_t eir_quat_a = 0x10;
constexpr uintptr_t eir_quat_b = 0x20;

// ios — камерная часть локального игрока
constexpr uintptr_t ios_rot        = 0x38; // eir
constexpr uintptr_t ios_stats      = 0x40; // ylw
constexpr uintptr_t ios_transform_a= 0x50; // Transform
constexpr uintptr_t ios_transform_b= 0x58; // Transform

// njh (ccg impl) -> gat -> gzx -> yjk<krm>
constexpr uintptr_t njh_registry   = 0x40; // gat
constexpr uintptr_t gat_store      = 0x30; // gzx
constexpr uintptr_t gzx_entities   = 0x18; // yjk<krm>
constexpr uintptr_t yjk_array      = 0x10; // krm[]
constexpr uintptr_t yjk_count      = 0x18; // int

// WeaponContext (не обфусцирован)
constexpr uintptr_t wctx_owner     = 0x00; // NetObjectId
constexpr uintptr_t wctx_team      = 0x08; // byte
constexpr uintptr_t wctx_hit_tf    = 0x10; // Transform — источник хит-рейкастов
constexpr uintptr_t wctx_recoil_ctx = 0x40; // RecoilContext.RecoilMult (float)
constexpr uintptr_t wctx_weapon    = 0x48; // qjf
constexpr uintptr_t wctx_time      = 0x8C; // float
constexpr uintptr_t wctx_was_hit   = 0x90; // bool

// RecoilState внутри WeaponContext (+0x68)
constexpr uintptr_t wctx_recoil          = 0x68;
constexpr uintptr_t rst_progress         = 0x00;
constexpr uintptr_t rst_prev_progress    = 0x04;
constexpr uintptr_t rst_prev_point       = 0x08; // Vector2
constexpr uintptr_t rst_point            = 0x10; // Vector2
constexpr uintptr_t rst_approach_delta   = 0x18;
constexpr uintptr_t rst_last_shot_time   = 0x1C; // не трогать: по нему считается темп стрельбы
}

// имена классов (обфусцированы, матчим по klass->name)
namespace cls {
constexpr const char* player = "ake";
constexpr const char* model  = "jgi";
}

// HumanBodyBones
enum class Bone : int {
    Hips = 0, LeftUpperLeg = 1, RightUpperLeg = 2, LeftLowerLeg = 3, RightLowerLeg = 4,
    LeftFoot = 5, RightFoot = 6, Spine = 7, Chest = 8, Neck = 9, Head = 10,
    LeftShoulder = 11, RightShoulder = 12, LeftUpperArm = 13, RightUpperArm = 14,
    LeftLowerArm = 15, RightLowerArm = 16, LeftHand = 17, RightHand = 18,
    LeftToes = 19, RightToes = 20, UpperChest = 54,
};

} // namespace off
