#include "jni_bridge.h"

#include <dlfcn.h>

#include <string>

#include "bridge.h"
#include "cfg_gen.h"
#include "frame.h"

namespace jbridge {
namespace {

JavaVM* g_vm = nullptr;

void jSetConfig(JNIEnv* env, jclass, jfloatArray arr) {
    const jsize n = env->GetArrayLength(arr);
    float tmp[cfg::kTotal];
    const jsize m = n < cfg::kTotal ? n : cfg::kTotal;
    env->GetFloatArrayRegion(arr, 0, m, tmp);
    bridge::setConfig(tmp, m);
}

jint jReadFrame(JNIEnv* env, jclass, jfloatArray arr) {
    float tmp[frame::kTotal];
    const int seq = bridge::readFrame(tmp, frame::kTotal);
    if (seq == 0) return 0;
    const jsize n = env->GetArrayLength(arr);
    env->SetFloatArrayRegion(arr, 0, n < frame::kTotal ? n : frame::kTotal, tmp);
    return seq;
}

void jPollCmd(JNIEnv* env, jclass, jfloatArray arr) {
    const bridge::Command c = bridge::pollCommand();
    float tmp[8] = {static_cast<float>(c.seq), c.lookActive, c.dx, c.dy, c.fireHeld, c.vibrate, c.holdFire, 0};
    env->SetFloatArrayRegion(arr, 0, 8, tmp);
}

void jSetInput(JNIEnv*, jclass, jfloat firing, jfloat looking, jfloat w, jfloat h, jfloat fps, jfloat menu) {
    bridge::Input in;
    in.userFiring = firing; in.userLooking = looking; in.viewW = w; in.viewH = h; in.fps = fps; in.menuOpen = menu;
    bridge::setInput(in);
}

jstring jStatus(JNIEnv* env, jclass) { return env->NewStringUTF(bridge::status().c_str()); }
jstring jLog(JNIEnv* env, jclass, jint lines) { return env->NewStringUTF(logTail(lines).c_str()); }
jstring jTool(JNIEnv* env, jclass, jint id) { return env->NewStringUTF(bridge::toolName(id).c_str()); }

const JNINativeMethod kMethods[] = {
    {"setConfig", "([F)V", reinterpret_cast<void*>(jSetConfig)},
    {"readFrame", "([F)I", reinterpret_cast<void*>(jReadFrame)},
    {"pollCmd", "([F)V", reinterpret_cast<void*>(jPollCmd)},
    {"setInput", "(FFFFFF)V", reinterpret_cast<void*>(jSetInput)},
    {"status", "()Ljava/lang/String;", reinterpret_cast<void*>(jStatus)},
    {"log", "(I)Ljava/lang/String;", reinterpret_cast<void*>(jLog)},
    {"toolName", "(I)Ljava/lang/String;", reinterpret_cast<void*>(jTool)},
};

jclass loadClass(JNIEnv* env, jobject loader, const char* name) {
    jclass lc = env->FindClass("java/lang/ClassLoader");
    jmethodID m = env->GetMethodID(lc, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring s = env->NewStringUTF(name);
    jobject c = env->CallObjectMethod(loader, m, s);
    env->DeleteLocalRef(s);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return static_cast<jclass>(c);
}

}  // namespace

JavaVM* vm() { return g_vm; }

bool start(JavaVM* vm) {
    if (!vm) return false;
    g_vm = vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
        attached = true;
    }
    bool ok = false;
    do {
        jclass at = env->FindClass("android/app/ActivityThread");
        if (!at) { env->ExceptionClear(); break; }
        jmethodID cur = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
        if (!cur) { env->ExceptionClear(); break; }
        jobject app = env->CallStaticObjectMethod(at, cur);
        if (!app || env->ExceptionCheck()) { env->ExceptionClear(); break; }
        jclass ac = env->GetObjectClass(app);
        jmethodID gcl = env->GetMethodID(ac, "getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject loader = env->CallObjectMethod(app, gcl);
        if (!loader) break;
        jclass nat = loadClass(env, loader, "rage.so2.Native");
        jclass boot = loadClass(env, loader, "rage.so2.Boot");
        if (!nat || !boot) { rlog("jni: rage.so2 classes not found in the app dex (native-only run)"); break; }
        if (env->RegisterNatives(nat, kMethods, sizeof(kMethods) / sizeof(kMethods[0])) != JNI_OK) {
            env->ExceptionClear();
            rlog("jni: RegisterNatives failed");
            break;
        }
        jmethodID st = env->GetStaticMethodID(boot, "start", "()V");
        if (!st) { env->ExceptionClear(); break; }
        env->CallStaticVoidMethod(boot, st);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); break; }
        ok = true;
        rlog("jni: Boot.start() called");
    } while (false);
    if (attached) vm->DetachCurrentThread();
    return ok;
}

}  // namespace jbridge
