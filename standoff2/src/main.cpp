// RageMod // Standoff 2 1.0.0 arm64 — внутренний чит, грузится любым инжектором
// в процесс com.axlebolt.standoff2 (Zygisk-модуль, ptrace-инжектор, frida gadget, репак apk).
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "mods.h"
#include "il2cpp.h"
#include "log.h"
#include "touch.h"
#include "ui.h"

namespace {

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

    LOGI("RageMod up (hooks %d/4)", mods::installed_count());
    return nullptr;
}

} // namespace

__attribute__((constructor)) static void entry() {
    pthread_t th;
    pthread_create(&th, nullptr, init, nullptr);
    pthread_detach(th);
}
