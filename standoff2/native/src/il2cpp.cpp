#include "il2cpp.h"

#include <dlfcn.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "common.h"

namespace il2 {
namespace {

struct Api {
    void* (*domain_get)() = nullptr;
    void** (*domain_get_assemblies)(void*, size_t*) = nullptr;
    void* (*assembly_get_image)(void*) = nullptr;
    const char* (*image_get_name)(void*) = nullptr;
    void* (*class_from_name)(void*, const char*, const char*) = nullptr;
    void* (*class_get_methods)(void*, void**) = nullptr;
    void* (*class_get_method_from_name)(void*, const char*, int) = nullptr;
    void* (*class_get_field_from_name)(void*, const char*) = nullptr;
    void* (*class_get_parent)(void*) = nullptr;
    void* (*class_get_type)(void*) = nullptr;
    bool (*class_is_subclass_of)(void*, void*, bool) = nullptr;
    size_t (*field_get_offset)(void*) = nullptr;
    void (*field_static_get_value)(void*, void*) = nullptr;
    void* (*object_get_class)(void*) = nullptr;
    void* (*type_get_object)(const void*) = nullptr;
    char* (*type_get_name)(const void*) = nullptr;
    void* (*string_new)(const char*) = nullptr;
    void* (*runtime_invoke)(void*, void*, void**, void**) = nullptr;
    void* (*thread_attach)(void*) = nullptr;
    uint32_t (*gchandle_new)(void*, bool) = nullptr;
    void* (*gchandle_get_target)(uint32_t) = nullptr;
    void (*gchandle_free)(uint32_t) = nullptr;
    void* (*array_new)(void*, size_t) = nullptr;
    uint32_t (*method_get_param_count)(const void*) = nullptr;
    const void* (*method_get_param)(const void*, uint32_t) = nullptr;
    const char* (*method_get_name)(const void*) = nullptr;
    void (*free_)(void*) = nullptr;
    void* (*value_box)(void*, void*) = nullptr;
    void* (*object_new)(void*) = nullptr;
};

Api g;
bool g_loaded = false;
std::mutex g_mu;
std::unordered_map<std::string, void*> g_classCache;
std::vector<void*> g_images;
Stat g_stat;

template <class T> bool bindSym(void* h, const char* name, T& out) {
    out = reinterpret_cast<T>(dlsym(h, name));
    if (!out) rlog("il2cpp: missing export %s", name);
    return out != nullptr;
}

void refreshImages() {
    g_images.clear();
    void* dom = g.domain_get();
    if (!dom) return;
    size_t n = 0;
    void** asms = g.domain_get_assemblies(dom, &n);
    for (size_t i = 0; i < n; i++) g_images.push_back(g.assembly_get_image(asms[i]));
}

std::string typeName(const void* t) {
    char* s = g.type_get_name(t);
    if (!s) return std::string();
    std::string r(s);
    if (g.free_) g.free_(s);
    return r;
}

}  // namespace

bool load() {
    if (g_loaded) return true;
    void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return false;
    bool ok = true;
    ok &= bindSym(h, "il2cpp_domain_get", g.domain_get);
    ok &= bindSym(h, "il2cpp_domain_get_assemblies", g.domain_get_assemblies);
    ok &= bindSym(h, "il2cpp_assembly_get_image", g.assembly_get_image);
    ok &= bindSym(h, "il2cpp_image_get_name", g.image_get_name);
    ok &= bindSym(h, "il2cpp_class_from_name", g.class_from_name);
    ok &= bindSym(h, "il2cpp_class_get_methods", g.class_get_methods);
    ok &= bindSym(h, "il2cpp_class_get_method_from_name", g.class_get_method_from_name);
    ok &= bindSym(h, "il2cpp_class_get_field_from_name", g.class_get_field_from_name);
    ok &= bindSym(h, "il2cpp_class_get_parent", g.class_get_parent);
    ok &= bindSym(h, "il2cpp_class_get_type", g.class_get_type);
    ok &= bindSym(h, "il2cpp_class_is_subclass_of", g.class_is_subclass_of);
    ok &= bindSym(h, "il2cpp_field_get_offset", g.field_get_offset);
    ok &= bindSym(h, "il2cpp_field_static_get_value", g.field_static_get_value);
    ok &= bindSym(h, "il2cpp_object_get_class", g.object_get_class);
    ok &= bindSym(h, "il2cpp_type_get_object", g.type_get_object);
    ok &= bindSym(h, "il2cpp_type_get_name", g.type_get_name);
    ok &= bindSym(h, "il2cpp_string_new", g.string_new);
    ok &= bindSym(h, "il2cpp_runtime_invoke", g.runtime_invoke);
    ok &= bindSym(h, "il2cpp_thread_attach", g.thread_attach);
    ok &= bindSym(h, "il2cpp_gchandle_new", g.gchandle_new);
    ok &= bindSym(h, "il2cpp_gchandle_get_target", g.gchandle_get_target);
    ok &= bindSym(h, "il2cpp_gchandle_free", g.gchandle_free);
    ok &= bindSym(h, "il2cpp_array_new", g.array_new);
    ok &= bindSym(h, "il2cpp_method_get_param_count", g.method_get_param_count);
    ok &= bindSym(h, "il2cpp_method_get_param", g.method_get_param);
    ok &= bindSym(h, "il2cpp_method_get_name", g.method_get_name);
    ok &= bindSym(h, "il2cpp_value_box", g.value_box);
    ok &= bindSym(h, "il2cpp_object_new", g.object_new);
    g.free_ = reinterpret_cast<void (*)(void*)>(dlsym(h, "il2cpp_free"));   // optional
    g_loaded = ok;
    return ok;
}

bool domainPresent() { return g_loaded && g.domain_get() != nullptr; }

bool vmReady() {
    if (!g_loaded) return false;
    void* dom = g.domain_get();
    if (!dom) return false;
    size_t n = 0;
    void** asms = g.domain_get_assemblies(dom, &n);
    if (!asms || n < 8) return false;
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_images.size() != n) refreshImages();
    // CoreModule must be resolvable, otherwise the VM is still bootstrapping
    for (void* img : g_images) {
        if (img && g.class_from_name(img, "UnityEngine", "Object")) return true;
    }
    return false;
}

void attachThread() {
    if (!g_loaded) return;
    void* dom = g.domain_get();
    if (dom) g.thread_attach(dom);
}

Klass* klass(const char* ns, const char* name) {
    if (!g_loaded) return nullptr;
    std::string key = std::string(ns) + "." + name;
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_classCache.find(key);
    if (it != g_classCache.end()) return it->second;
    if (g_images.empty()) refreshImages();
    void* found = nullptr;
    for (void* img : g_images) {
        if (!img) continue;
        found = g.class_from_name(img, ns, name);
        if (found) break;
    }
    if (found) g_classCache[key] = found;   // negative results are not cached (late-loaded assemblies)
    return found;
}

Method* methodIn(Klass* k, const char* name, int argc) {
    return k ? g.class_get_method_from_name(k, name, argc) : nullptr;
}

Method* method(const char* ns, const char* cls, const char* name, int argc) {
    return methodIn(klass(ns, cls), name, argc);
}

Method* methodSig(const char* ns, const char* cls, const char* name, std::initializer_list<const char*> params) {
    Klass* k = klass(ns, cls);
    for (Klass* c = k; c; c = g.class_get_parent(c)) {
        void* it = nullptr;
        while (void* m = g.class_get_methods(c, &it)) {
            if (std::string(g.method_get_name(m)) != name) continue;
            if (g.method_get_param_count(m) != params.size()) continue;
            bool match = true;
            uint32_t i = 0;
            for (const char* want : params) {
                if (typeName(g.method_get_param(m, i++)) != want) { match = false; break; }
            }
            if (match) return m;
        }
    }
    return nullptr;
}

Field* fieldIn(Klass* k, const char* name) {
    for (Klass* c = k; c; c = g.class_get_parent(c)) {
        if (void* f = g.class_get_field_from_name(c, name)) return f;
    }
    return nullptr;
}

Field* field(const char* ns, const char* cls, const char* name) { return fieldIn(klass(ns, cls), name); }

int fieldOffset(Field* f) { return f ? static_cast<int>(g.field_get_offset(f)) : -1; }

Klass* classOf(Obj* o) { return o ? g.object_get_class(o) : nullptr; }

bool isInstance(Obj* o, Klass* k) {
    if (!o || !k) return false;
    return g.class_is_subclass_of(g.object_get_class(o), k, false) || g.object_get_class(o) == k;
}

Obj* newObject(Klass* k) { return k ? g.object_new(k) : nullptr; }
Obj* newString(const char* s) { return g.string_new(s); }

std::string toUtf8(Obj* str) {
    if (!str) return std::string();
    int32_t n = *reinterpret_cast<int32_t*>(static_cast<char*>(str) + 0x10);
    if (n <= 0 || n > 4096) return std::string();
    const uint16_t* c = reinterpret_cast<const uint16_t*>(static_cast<char*>(str) + 0x14);
    std::string out;
    out.reserve(n);
    for (int32_t i = 0; i < n; i++) {
        uint32_t u = c[i];
        if (u >= 0xD800 && u < 0xDC00 && i + 1 < n) {
            uint32_t lo = c[++i];
            u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
        }
        if (u < 0x80) out += static_cast<char>(u);
        else if (u < 0x800) { out += static_cast<char>(0xC0 | (u >> 6)); out += static_cast<char>(0x80 | (u & 0x3F)); }
        else if (u < 0x10000) { out += static_cast<char>(0xE0 | (u >> 12)); out += static_cast<char>(0x80 | ((u >> 6) & 0x3F)); out += static_cast<char>(0x80 | (u & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (u >> 18)); out += static_cast<char>(0x80 | ((u >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((u >> 6) & 0x3F)); out += static_cast<char>(0x80 | (u & 0x3F)); }
    }
    return out;
}

Obj* typeObject(Klass* k) { return k ? g.type_get_object(g.class_get_type(k)) : nullptr; }
Obj* newArray(Klass* elem, size_t n) { return g.array_new(elem, n); }
size_t arrayLen(Obj* arr) { return arr ? *reinterpret_cast<size_t*>(static_cast<char*>(arr) + 0x18) : 0; }

Obj* invoke(Method* m, void* self, void** args, bool* threw) {
    void* exc = nullptr;
    void* r = g.runtime_invoke(m, self, args, &exc);
    if (threw) *threw = exc != nullptr;
    return exc ? nullptr : r;
}

Obj* box(Klass* k, void* value) { return g.value_box(k, value); }
void staticGet(Field* f, void* out) { g.field_static_get_value(f, out); }
Obj* staticGetObj(Field* f) { void* o = nullptr; g.field_static_get_value(f, &o); return o; }

void Ref::set(Obj* o) {
    reset();
    if (o) h_ = g.gchandle_new(o, false);
}
void Ref::reset() {
    if (h_) { g.gchandle_free(h_); h_ = 0; }
}
Obj* Ref::get() const { return h_ ? g.gchandle_get_target(h_) : nullptr; }

Stat& stat() { return g_stat; }
void noteFail(const std::string& what) {
    g_stat.fail++;
    if (g_stat.failed.size() < 600) { g_stat.failed += what; g_stat.failed += "; "; }
    rlog("resolve FAILED: %s", what.c_str());
}

}  // namespace il2
