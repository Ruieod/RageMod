// Anti-aim. Два пути отправки угла:
//  * fake — через CUserCmd, камера на месте. Нужен sigs::g.usercmd_ready().
//  * real — записью в dwViewAngles. Работает всегда, но крутит и камеру,
//           то есть годится только для дм/локалки.
#pragma once
#include <cstdint>

#include "vmath.h"

namespace antiaim {

bool enabled();                                  // режим не Off и бинд активен
Vec3 fake_angles(const Vec3& real, float yaw_to_target);

// зовётся каждый кадр из game::tick до аима
void run(const Vec3& eye, bool has_target, const Vec3& target_point);

// зовётся из хука на CUserCmd, когда он появится
void on_usercmd(void* usercmd);

bool via_usercmd();                              // true, если идём честным путём

} // namespace antiaim
