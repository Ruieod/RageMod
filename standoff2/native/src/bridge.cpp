#include "bridge.h"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "cfg_gen.h"
#include "frame.h"

namespace bridge {
namespace {
std::mutex g_mu;
std::vector<float> g_cfg(cfg::kTotal, 0.f);
float g_frame[2][frame::kTotal];
int g_front = 0;
uint32_t g_frameSeq = 0;
Input g_in;
Command g_cmd;
std::string g_status;
std::unordered_map<int, std::string> g_tools;
}  // namespace

void setConfig(const float* v, int n) {
    std::lock_guard<std::mutex> lk(g_mu);
    int m = n < cfg::kTotal ? n : cfg::kTotal;
    std::memcpy(g_cfg.data(), v, sizeof(float) * m);
}

void snapshotConfig(float* out, int n) {
    std::lock_guard<std::mutex> lk(g_mu);
    int m = n < cfg::kTotal ? n : cfg::kTotal;
    std::memcpy(out, g_cfg.data(), sizeof(float) * m);
}

void publishFrame(const float* data, int n) {
    std::lock_guard<std::mutex> lk(g_mu);
    int back = 1 - g_front;
    int m = n < frame::kTotal ? n : frame::kTotal;
    std::memcpy(g_frame[back], data, sizeof(float) * m);
    g_frameSeq++;
    g_frame[back][frame::H_SEQ] = static_cast<float>(g_frameSeq);
    g_front = back;
}

int readFrame(float* out, int n) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_frameSeq == 0) return 0;
    int m = n < frame::kTotal ? n : frame::kTotal;
    std::memcpy(out, g_frame[g_front], sizeof(float) * m);
    return static_cast<int>(g_frameSeq);
}

void setInput(const Input& in) { std::lock_guard<std::mutex> lk(g_mu); g_in = in; }
Input getInput() { std::lock_guard<std::mutex> lk(g_mu); return g_in; }

void pushCommand(bool lookActive, float dx, float dy, bool fireHeld) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_cmd.seq++;
    g_cmd.lookActive = lookActive ? 1.f : 0.f;
    g_cmd.dx += dx;
    g_cmd.dy += dy;
    g_cmd.fireHeld = fireHeld ? 1.f : 0.f;
}

void pushVibrate() { std::lock_guard<std::mutex> lk(g_mu); g_cmd.vibrate = 1.f; }

Command pollCommand() {
    std::lock_guard<std::mutex> lk(g_mu);
    Command c = g_cmd;
    g_cmd.dx = 0;
    g_cmd.dy = 0;
    g_cmd.vibrate = 0;
    return c;
}

void setStatus(const std::string& s) { std::lock_guard<std::mutex> lk(g_mu); g_status = s; }
std::string status() { std::lock_guard<std::mutex> lk(g_mu); return g_status; }

void setToolName(int id, const std::string& s) { std::lock_guard<std::mutex> lk(g_mu); g_tools[id] = s; }
std::string toolName(int id) {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_tools.find(id);
    return it == g_tools.end() ? std::string() : it->second;
}

}  // namespace bridge
