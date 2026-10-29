#pragma once
#include <string>

namespace game {

bool init();                 // resolve game classes (after unity::init). false if the character source is missing
void tick();                 // once per Unity frame, on the Unity main thread
std::string describe();      // one-paragraph state dump for the debug panel

}  // namespace game
