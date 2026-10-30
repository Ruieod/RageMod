// То, что нельзя взять из cs2-dumper: индекс метода в vtable и смещения
// внутрь CUserCmd. Правятся в ragemod_sigs.ini рядом с конфигом и
// перечитываются по кнопке — не надо пересобирать dll ради одного числа.
#pragma once
#include <cstdint>

namespace sigs {

struct Table {
    uintptr_t csgo_input = 0;      // CCSGOInput*, из dwCSGOInput

    int getusercmd_index = -1;     // индекс GetUserCmd(slot) в vtable CCSGOInput
    int usercmd_pb = -1;           // CUserCmd -> CSGOUserCmdPB
    int pb_base = -1;              // CSGOUserCmdPB -> CBaseUserCmdPB
    int base_viewangles = -1;      // CBaseUserCmdPB -> CMsgQAngle*
    int qangle_x = -1;             // CMsgQAngle -> float x (y,z следом)
    int base_subtick = -1;         // CBaseUserCmdPB -> subtick moves (0 = не трогаем)

    bool usercmd_ready() const {
        return csgo_input && getusercmd_index >= 0 && usercmd_pb >= 0 && pb_base >= 0 &&
               base_viewangles >= 0 && qangle_x >= 0;
    }
};

extern Table g;

const char* ini_path();
bool load();                       // ini + то, что резолвится сканом
bool save_template();              // кладёт ini-заготовку с комментариями
int dump_input_vtable();           // -> ragemod_vtable.txt, вернёт число строк
const char* status();              // одна строка для меню

} // namespace sigs
