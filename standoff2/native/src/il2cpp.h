#pragma once
// Thin runtime bridge over libil2cpp's public C API.
// Everything is resolved by *name* at runtime - no RVAs, no pattern scans.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace il2 {

using Obj = void;
using Klass = void;
using Method = void;
using Field = void;
using Type = void;

// ---- bring-up -------------------------------------------------------------
bool load();            // dlopen/dlsym libil2cpp.so, true when every required export is bound
bool domainPresent();   // il2cpp_init has at least created the domain (assemblies may still be loading - do not walk them yet)
bool vmReady();         // il2cpp_init finished (domain exists and corlib is loaded)
void attachThread();    // attach the calling thread to the VM (safe to call repeatedly)

// ---- lookup (all cached) --------------------------------------------------
Klass* klass(const char* ns, const char* name);
Method* method(const char* ns, const char* cls, const char* name, int argc);
// exact overload by parameter type names, e.g. {"UnityEngine.Vector3","System.Single"}
Method* methodSig(const char* ns, const char* cls, const char* name, std::initializer_list<const char*> params);
Method* methodIn(Klass* k, const char* name, int argc);      // walks parents
Field* field(const char* ns, const char* cls, const char* name);
Field* fieldIn(Klass* k, const char* name);                   // walks parents
int fieldOffset(Field* f);

inline void* fnPtr(Method* m) { return m ? *reinterpret_cast<void**>(m) : nullptr; }
inline void** fnSlot(Method* m) { return reinterpret_cast<void**>(m); }

// ---- objects --------------------------------------------------------------
Klass* classOf(Obj* o);
bool isInstance(Obj* o, Klass* k);
Obj* newObject(Klass* k);           // allocates, does NOT run a constructor
Obj* newString(const char* utf8);
std::string toUtf8(Obj* managedString);
Obj* typeObject(Klass* k);          // System.Type for typeof(k)
Obj* newArray(Klass* elem, size_t n);
size_t arrayLen(Obj* arr);
inline void** arrayData(Obj* arr) { return reinterpret_cast<void**>(static_cast<char*>(arr) + 0x20); }
Obj* invoke(Method* m, void* self, void** args, bool* threw = nullptr);
Obj* box(Klass* k, void* value);

template <class T> inline T read(Obj* o, int off) { return *reinterpret_cast<T*>(static_cast<char*>(o) + off); }
template <class T> inline void write(Obj* o, int off, T v) { *reinterpret_cast<T*>(static_cast<char*>(o) + off) = v; }
void staticGet(Field* f, void* out);
Obj* staticGetObj(Field* f);

// ---- GC handles: keep managed objects alive across frames ------------------
class Ref {
public:
    Ref() = default;
    explicit Ref(Obj* o) { set(o); }
    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;
    Ref(Ref&& o) noexcept : h_(o.h_) { o.h_ = 0; }
    Ref& operator=(Ref&& o) noexcept { if (this != &o) { reset(); h_ = o.h_; o.h_ = 0; } return *this; }
    ~Ref() { reset(); }
    void set(Obj* o);
    void reset();
    Obj* get() const;
    explicit operator bool() const { return h_ != 0; }
private:
    uint32_t h_ = 0;
};

// ---- typed call thunks ----------------------------------------------------
// Fn<R(A...)>: A... includes `this` as the first argument for instance methods.
template <class Sig> struct Fn;
template <class R, class... A> struct Fn<R(A...)> {
    Method* m = nullptr;
    void* p = nullptr;
    bool bind(Method* mm) { m = mm; p = fnPtr(mm); return p != nullptr; }
    bool ok() const { return p != nullptr; }
    R operator()(A... a) const { return reinterpret_cast<R (*)(A..., Method*)>(p)(a..., m); }
};

// resolution bookkeeping, shown in the debug tab
struct Stat { int ok = 0; int fail = 0; std::string failed; };
Stat& stat();
void noteFail(const std::string& what);

}  // namespace il2

#define BIND(var, ns, cls, name, argc) \
    do { if (!(var).bind(::il2::method(ns, cls, name, argc))) ::il2::noteFail(std::string(cls) + "::" + name); else ::il2::stat().ok++; } while (0)
#define BIND_SIG(var, ns, cls, name, ...) \
    do { if (!(var).bind(::il2::methodSig(ns, cls, name, {__VA_ARGS__}))) ::il2::noteFail(std::string(cls) + "::" + name + "(sig)"); else ::il2::stat().ok++; } while (0)
