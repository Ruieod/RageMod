#include "common.h"

#include <chrono>
#include <deque>
#include <mutex>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace {
std::mutex g_logMu;
std::deque<std::string> g_ring;
}  // namespace

void rlog(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "RageMod", "%s", buf);
#else
    fprintf(stderr, "[RageMod] %s\n", buf);
#endif
    std::lock_guard<std::mutex> lk(g_logMu);
    g_ring.emplace_back(buf);
    if (g_ring.size() > 200) g_ring.pop_front();
}

std::string logTail(int maxLines) {
    std::lock_guard<std::mutex> lk(g_logMu);
    std::string out;
    int start = static_cast<int>(g_ring.size()) - maxLines;
    if (start < 0) start = 0;
    for (size_t i = static_cast<size_t>(start); i < g_ring.size(); i++) { out += g_ring[i]; out += '\n'; }
    return out;
}

namespace {
bool g_fake = false;
double g_fakeT = 0;
}  // namespace

void testClock(bool enable, double t) { g_fake = enable; g_fakeT = t; }

double nowSec() {
    using namespace std::chrono;
    if (g_fake) return g_fakeT;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}
