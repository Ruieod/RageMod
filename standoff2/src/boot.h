#pragma once

namespace ragemod {
const char* target_package();
bool in_target_process();   // /proc/self/cmdline == target_package()
void start();               // идемпотентно: поднимает init-поток один раз
} // namespace ragemod
