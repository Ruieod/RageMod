#pragma once
#include <string>

namespace game {

bool init();                 // resolve game classes (after unity::init). false if the character source is missing
void tick();                 // once per Unity frame, on the Unity main thread
void preRender(void* cam);   // render hooks (main.cpp): cam = null from the canvas pass, else the camera about to cull
void postRender(void* cam);  // after `cam` rendered
std::string describe();      // one-paragraph state dump for the debug panel

}  // namespace game
