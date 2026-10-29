// RageMod // Standoff 2 1.0.0 arm64 — внутренний чит.
// Грузится либо Zygisk-модулем (src/zygisk_entry.cpp), либо любым инжектором,
// который делает dlopen этой либы в процесс com.axlebolt.standoff2.
#include "boot.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <cstring>

#include "config.h"
#include "il2cpp.h"
#include "log.h"
#include "mods.h"
#include "touch.h"
#include "ui.h"

namespace ragemod {
namespace {

std::atomic<bool> g_started{false};

void* init(void*) {
    pthread_setname_np(pthread_self(), "rm-init");

    if (!il2cpp::wait_for_module(60000)) {
        LOGE("libunity.so так и не замаппилась");
        return nullptr;
    }
    // даём рантайму доинициализироваться: хуки ставим по уже прогретому коду
    usleep(4 * 1000 * 1000);

    config::load();
    mods::install();
    ui::install();
    touch::start();

    LOGI("RageMod up (hooks %d/7)", mods::installed_count());
    return nullptr;
}

} // namespace

const char* target_package() { return "com.axlebolt.standoff2"; }

bool in_target_process() {
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buf[256] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return false;
    return std::strcmp(buf, target_package()) == 0;
}

void start() {
    if (g_started.exchange(true)) return;
    pthread_t th;
    pthread_create(&th, nullptr, init, nullptr);
    pthread_detach(th);
}

} // namespace ragemod

// инжектор просто делает dlopen — поднимаемся сами, но только в своём процессе
__attribute__((constructor)) static void entry() {
    if (ragemod::in_target_process()) ragemod::start();
}
