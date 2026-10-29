// Эта же либа работает как Zygisk-модуль: Magisk грузит её в каждый форк зиготы,
// поэтому в чужих процессах сразу выгружаемся и ничего не делаем.
#include <cstring>

#include "boot.h"
#include "log.h"
#include "zygisk.hpp"

namespace {

class RageModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        api_ = api;
        env_ = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        target_ = false;
        if (args && args->nice_name) {
            const char* name = env_->GetStringUTFChars(args->nice_name, nullptr);
            if (name) {
                target_ = std::strcmp(name, ragemod::target_package()) == 0;
                env_->ReleaseStringUTFChars(args->nice_name, name);
            }
        }
        if (!target_) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs*) override {
        if (!target_) return;
        LOGI("zygisk: %s, поднимаемся", ragemod::target_package());
        ragemod::start();
    }

private:
    zygisk::Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool target_ = false;
};

} // namespace

REGISTER_ZYGISK_MODULE(RageModule)
