// база: tools/update_offsets.py из github.com/a2x/cs2-dumper, дамп от 2026-10-23
// client_dll-глобалы пересняты статикой сигнатурами cs2-dumper с client.dll
// билда 2026-10-28 (PE TimeDateStamp), т.к. публичного дампа под него нет.
// Схема (32 поля) сверена с бинарём — не менялась, значения ниже валидны.
// Используемые кодом глобалы пересняты; dwGlobalVars/dwGameRules/dwPlantedC4/
// dwSensitivity/dwGlowManager в коде не используются и оставлены от 10-23.
// Кнопки (buttons::*) снять статикой нельзя — список строится в рантайме;
// на 10-28 они невалидны. attack/jump нужны только для auto-fire/trigger/bhop,
// которые по умолчанию выключены. Для них нужен прогон cs2-dumper по живой игре.
#pragma once
#include <cstdint>

namespace off {
namespace client_dll {
constexpr uintptr_t dwEntityList = 0x271eb08;
constexpr uintptr_t dwLocalPlayerController = 0x25413f8;
constexpr uintptr_t dwLocalPlayerPawn = 0x25698c8;
constexpr uintptr_t dwViewMatrix = 0x256f1d0;
constexpr uintptr_t dwViewAngles = 0x2580008;
constexpr uintptr_t dwCSGOInput = 0x257f980;
constexpr uintptr_t dwGlobalVars = 0x2232088;
constexpr uintptr_t dwGameRules = 0x2567188;
constexpr uintptr_t dwPlantedC4 = 0x24cd930;
constexpr uintptr_t dwSensitivity = 0x25649d8;
constexpr uintptr_t dwSensitivity_sensitivity = 0x58;
constexpr uintptr_t dwGameEntitySystem = 0x271eb08;
constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2120;
constexpr uintptr_t dwGlowManager = 0x2563eb0;
}
namespace engine2_dll {
constexpr uintptr_t dwNetworkGameClient = 0x9b88c0;
constexpr uintptr_t dwNetworkGameClient_localPlayer = 0xf8;
constexpr uintptr_t dwNetworkGameClient_maxClients = 0x240;
constexpr uintptr_t dwNetworkGameClient_signOnState = 0x230;
constexpr uintptr_t dwBuildNumber = 0x6ba02c;
constexpr uintptr_t dwWindowHeight = 0x9bcff4;
constexpr uintptr_t dwWindowWidth = 0x9bcff0;
}
namespace buttons {
constexpr uintptr_t attack = 0x2235ed0;
constexpr uintptr_t attack2 = 0x2235f60;
constexpr uintptr_t jump = 0x22363e0;
constexpr uintptr_t duck = 0x2236470;
constexpr uintptr_t reload = 0x2235e40;
}
namespace C_BaseEntity {
constexpr uintptr_t m_iHealth = 0x34c;
constexpr uintptr_t m_iTeamNum = 0x3e7;
constexpr uintptr_t m_pGameSceneNode = 0x330;
constexpr uintptr_t m_lifeState = 0x354;
constexpr uintptr_t m_fFlags = 0x3f4;
constexpr uintptr_t m_vecVelocity = 0x430;
}
namespace CGameSceneNode {
constexpr uintptr_t m_vecAbsOrigin = 0xc8;
constexpr uintptr_t m_bDormant = 0x103;
}
namespace CSkeletonInstance {
constexpr uintptr_t m_modelState = 0x140;
}
namespace CBasePlayerController {
constexpr uintptr_t m_hPawn = 0x6bc;
constexpr uintptr_t m_iszPlayerName = 0x6fc;
}
namespace CCSPlayerController {
constexpr uintptr_t m_hPlayerPawn = 0x92c;
constexpr uintptr_t m_bPawnIsAlive = 0x934;
constexpr uintptr_t m_iPawnHealth = 0x938;
constexpr uintptr_t m_sSanitizedPlayerName = 0x878;
constexpr uintptr_t m_iPawnArmor = 0x93c;
constexpr uintptr_t m_bPawnHasHelmet = 0x941;
}
namespace C_CSPlayerPawnBase {
constexpr uintptr_t m_flFlashDuration = 0x1510;
constexpr uintptr_t m_flFlashMaxAlpha = 0x150c;
}
namespace C_CSPlayerPawn {
constexpr uintptr_t m_angEyeAngles = 0x3600;
constexpr uintptr_t m_iShotsFired = 0x1eb8;
constexpr uintptr_t m_ArmorValue = 0x1ed0;
constexpr uintptr_t m_bIsScoped = 0x1ea0;
constexpr uintptr_t m_bIsDefusing = 0x1ea2;
constexpr uintptr_t m_bIsWalking = 0x1e80;
constexpr uintptr_t m_iIDEntIndex = 0x36dc;
constexpr uintptr_t m_entitySpottedState = 0x1e88;
}
namespace C_BasePlayerPawn {
constexpr uintptr_t m_pWeaponServices = 0x12f0;
constexpr uintptr_t m_pMovementServices = 0x1330;
}
namespace CPlayer_WeaponServices {
constexpr uintptr_t m_hActiveWeapon = 0x60;
}
namespace C_BaseModelEntity {
constexpr uintptr_t m_Glow = 0xde8;
constexpr uintptr_t m_clrRender = 0xca0;
constexpr uintptr_t m_vecViewOffset = 0xf60;
}
namespace CGlowProperty {
constexpr uintptr_t m_glowColorOverride = 0x40;
constexpr uintptr_t m_bGlowing = 0x51;
constexpr uintptr_t m_iGlowType = 0x30;
constexpr uintptr_t m_iGlowTeam = 0x34;
}
namespace EntitySpottedState_t {
constexpr uintptr_t m_bSpotted = 0x8;
constexpr uintptr_t m_bSpottedByMask = 0xc;
}
namespace C_BaseFlex {
}
namespace C_EconEntity {
constexpr uintptr_t m_AttributeManager = 0x1290;
constexpr uintptr_t m_nFallbackPaintKit = 0x18a8;
constexpr uintptr_t m_flFallbackWear = 0x18b0;
constexpr uintptr_t m_nFallbackSeed = 0x18ac;
constexpr uintptr_t m_nFallbackStatTrak = 0x18b4;
}
namespace C_AttributeContainer {
constexpr uintptr_t m_Item = 0x50;
}
namespace C_EconItemView {
constexpr uintptr_t m_iItemDefinitionIndex = 0x1ba;
constexpr uintptr_t m_iEntityQuality = 0x1bc;
constexpr uintptr_t m_bInitialized = 0x1e8;
constexpr uintptr_t m_iAccountID = 0x1d8;
}
namespace C_BasePlayerWeapon {
constexpr uintptr_t m_nNextPrimaryAttackTick = 0x1918;
constexpr uintptr_t m_iClip1 = 0x1928;
constexpr uintptr_t m_iClip2 = 0x192c;
}
namespace C_CSWeaponBase {
constexpr uintptr_t m_bUIWeapon = 0x1b22;
}
namespace C_BaseCombatCharacter {
}
namespace C_PlantedC4 {
constexpr uintptr_t m_bBombTicking = 0x1288;
constexpr uintptr_t m_flC4Blow = 0x12b8;
constexpr uintptr_t m_bBeingDefused = 0x12c4;
}
} // namespace off
