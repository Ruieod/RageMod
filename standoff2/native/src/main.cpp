// Entry: libragemod.so is pulled in as a DT_NEEDED dependency of the game's libmain.so (see tools/patch_apk.py),
// so the constructor below runs while the game is still booting. A worker thread then waits for il2cpp and the
// Java application, installs the per-frame hook and starts the overlay.
#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>

#include <atomic>
#include <thread>

#include "bridge.h"
#include "common.h"
#include "game.h"
#include "il2cpp.h"
#include "jni_bridge.h"
#include "unity.h"

namespace {

std::atomic<bool> g_started{false};
std::atomic<bool> g_javaUp{false};
JavaVM* g_vm = nullptr;

// ---- hooks: swap MethodInfo::methodPointer of methods the engine invokes by MethodInfo every frame ----
void* swapMethod(const char* ns, const char* cls, const char* name, int argc, void* repl) {
    il2::Method* m = il2::method(ns, cls, name, argc);
    if (!m) return nullptr;
    void** slot = il2::fnSlot(m);
    void* original = slot[0];
    if (!original) return nullptr;
    slot[0] = repl;
    if (slot[1] == original) slot[1] = repl;   // virtualMethodPointer
    rlog("hook: %s.%s::%s -> %p (orig %p)", ns, cls, name, repl, original);
    return original;
}

template <int N> struct TickHook {
    static void (*orig)(void*, void*);
    static void hook(void* self, void* mi) {
        if (orig) orig(self, mi);
        game::tick();
    }
};
template <int N> void (*TickHook<N>::orig)(void*, void*) = nullptr;

template <int N> bool installHook(const char* ns, const char* cls, const char* name) {
    // orig is published before the swap: the Unity thread may enter the hook right after the store
    il2::Method* m = il2::method(ns, cls, name, 0);
    if (!m || !il2::fnPtr(m)) return false;
    TickHook<N>::orig = reinterpret_cast<void (*)(void*, void*)>(il2::fnPtr(m));
    return swapMethod(ns, cls, name, 0, reinterpret_cast<void*>(&TickHook<N>::hook)) != nullptr;
}

// ---- render hooks for the silent camera lock (static [RequiredByNativeCode] methods) ----
void (*g_origCanvases)(void*) = nullptr;
void (*g_origPreCull)(void*, void*) = nullptr;
void (*g_origPostRender)(void*, void*) = nullptr;
void canvasesHook(void* mi) {   // PostLateUpdate: after every Update / LateUpdate / ECS presentation, before rendering
    if (g_origCanvases) g_origCanvases(mi);
    game::preRender(nullptr);
}
void preCullHook(void* cam, void* mi) {
    game::preRender(cam);
    if (g_origPreCull) g_origPreCull(cam, mi);
}
void postRenderHook(void* cam, void* mi) {
    if (g_origPostRender) g_origPostRender(cam, mi);
    game::postRender(cam);
}

template <class F> int installRender(const char* ns, const char* cls, const char* name, int argc, F& orig, F repl) {
    il2::Method* m = il2::method(ns, cls, name, argc);
    if (!m || !il2::fnPtr(m)) return 0;
    orig = reinterpret_cast<F>(il2::fnPtr(m));
    return swapMethod(ns, cls, name, argc, reinterpret_cast<void*>(repl)) ? 1 : 0;
}

JavaVM* findVm() {
    using Fn = jint (*)(JavaVM**, jsize, jsize*);
    Fn fn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
    if (!fn) {
        for (const char* lib : {"libnativehelper.so", "libart.so"}) {
            void* h = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
            if (h) { fn = reinterpret_cast<Fn>(dlsym(h, "JNI_GetCreatedJavaVMs")); if (fn) break; }
        }
    }
    if (!fn) return nullptr;
    JavaVM* vm = nullptr;
    jsize n = 0;
    if (fn(&vm, 1, &n) != JNI_OK || n < 1) return nullptr;
    return vm;
}

void worker() {
    rlog("RageMod worker started (pid %d)", getpid());
    // 1) wait for libil2cpp + a booted VM. The domain appears early in il2cpp_init while assemblies are still being
    //    registered on the Unity thread, so after it shows up we give the runtime a few seconds before walking anything.
    double domainSeen = 0;
    for (int i = 0; i < 6000; i++) {
        if (il2::load() && il2::domainPresent()) {
            if (domainSeen == 0) { domainSeen = nowSec(); rlog("il2cpp domain present, letting the runtime settle"); }
            if (nowSec() - domainSeen > 4.0 && il2::vmReady()) break;
        }
        usleep(100 * 1000);
        if (i == 5999) { rlog("il2cpp never came up, giving up"); return; }
    }
    il2::attachThread();
    rlog("il2cpp ready");

    // 2) Java side (needs the Application, which exists long before il2cpp is up)
    for (int i = 0; i < 100 && !g_javaUp; i++) {
        if (!g_vm) g_vm = findVm();
        if (g_vm && jbridge::start(g_vm)) g_javaUp = true;
        else usleep(300 * 1000);
    }

    // 3) game layer
    const bool unityOk = unity::init();
    const bool gameOk = game::init();
    rlog("unity=%d game=%d", unityOk, gameOk);

    // 4) frame hook(s): install on every candidate that exists; game::tick() de-duplicates per frame
    int hooks = 0;
    hooks += installHook<0>("UnityEngine.EventSystems", "EventSystem", "Update");
    hooks += installHook<1>("UnityEngine.UI", "ScrollRect", "LateUpdate");
    rlog("frame hooks installed: %d", hooks);
    int render = 0;
    render += installRender("UnityEngine", "Canvas", "SendWillRenderCanvases", 0, g_origCanvases, &canvasesHook);
    render += installRender("UnityEngine", "Camera", "FireOnPreCull", 1, g_origPreCull, &preCullHook);
    render += installRender("UnityEngine", "Camera", "FireOnPostRender", 1, g_origPostRender, &postRenderHook);
    rlog("render hooks installed: %d", render);
    bridge::setStatus(hooks ? "hooks installed, waiting for the first frame" : "NO FRAME HOOK could be installed");
}

void startOnce() {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true)) return;
    std::thread(worker).detach();
}

}  // namespace

__attribute__((constructor)) static void ragemod_ctor() {
    rlog("libragemod loaded");
    startOnce();
}

// Used when the library is loaded through System.loadLibrary instead of DT_NEEDED
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    startOnce();
    return JNI_VERSION_1_6;
}
