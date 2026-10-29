#pragma once
#include <jni.h>

namespace jbridge {
// Hands the JavaVM to the Java side: loads rage.so2.Boot from the app class loader, registers natives, calls Boot.start().
// Safe to call from any thread; returns false while the Application / class loader is not available yet.
bool start(JavaVM* vm);
JavaVM* vm();
}  // namespace jbridge
