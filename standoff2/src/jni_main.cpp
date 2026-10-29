// Загрузка без рута и без правки dex: наша либа кладётся в apk под именем libmain.so,
// оригинальный Unity-шный libmain.so переименовывается в libmain_orig.so.
// Unity зовёт System.loadLibrary("main") -> линкер поднимает нас, мы стартуем чит и
// пробрасываем JNI_OnLoad в оригинал. Других экспортов у libmain.so нет (проверено
// readelf: ровно один символ JNI_OnLoad), так что подмена ничего не ломает.
#include <dlfcn.h>
#include <jni.h>

#include "boot.h"
#include "log.h"

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    if (ragemod::in_target_process()) ragemod::start();

    void* orig = dlopen("libmain_orig.so", RTLD_NOW | RTLD_LOCAL);
    if (!orig) {
        LOGE("libmain_orig.so не открылась: %s", dlerror());
        return JNI_VERSION_1_6;
    }
    auto fn = (jint (*)(JavaVM*, void*))dlsym(orig, "JNI_OnLoad");
    if (!fn) {
        LOGE("в libmain_orig.so нет JNI_OnLoad");
        return JNI_VERSION_1_6;
    }
    return fn(vm, reserved);
}
