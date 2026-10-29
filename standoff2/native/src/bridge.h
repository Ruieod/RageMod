#pragma once
// Cross-thread mailbox between the Unity thread (native tick) and the Java side (UI thread via JNI).
#include <cstdint>
#include <string>

#include "common.h"

namespace bridge {

// ---- config (Java -> Unity) ----
void setConfig(const float* v, int n);
void snapshotConfig(float* out, int n);       // copies the latest config into `out`

// ---- frame (Unity -> Java) ----
void publishFrame(const float* data, int n);
int readFrame(float* out, int n);             // returns seq of the copy (0 if none)

// ---- live inputs (Java -> Unity), refreshed every Java frame ----
struct Input {
    float userFiring = 0;       // a real finger is on the fire button
    float userLooking = 0;      // a real finger is on the look area
    float viewW = 0, viewH = 0; // overlay size in px
    float fps = 0;
    float menuOpen = 0;
};
void setInput(const Input& in);
Input getInput();

// ---- commands (Unity -> Java): accumulated until Java polls ----
struct Command {
    uint32_t seq = 0;
    float lookActive = 0;
    float dx = 0, dy = 0;       // accumulated finger delta since the last poll
    float fireHeld = 0;
    float vibrate = 0;
};
void pushCommand(bool lookActive, float dx, float dy, bool fireHeld);
void pushVibrate();
Command pollCommand();

// ---- status text for the debug panel ----
void setStatus(const std::string& s);
std::string status();

// weapon / tool names by character id (debug + ESP text)
void setToolName(int id, const std::string& s);
std::string toolName(int id);

}  // namespace bridge
