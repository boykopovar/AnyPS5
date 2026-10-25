#include "prx/libSceNpGameIntent/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value, const char* what) {
    if (value) return;
    std::printf("NpGameIntent: %s\n", what);
    std::abort();
}

template <typename TError, typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const TError&) {
        return true;
    }
    return false;
}

int main() {
    constexpr int kIntentNotFound = -2141898746;
    constexpr int kValueNotFound = -2141898745;

    Require(Throws<std::invalid_argument>([] { sceNpGameIntentInitParamInit(nullptr); }),
            "null param init did not throw");

    NpGameIntentInitParam initParam{};
    Require(sceNpGameIntentInitParamInit(&initParam) == 0, "param init failed");
    Require(initParam.size == sizeof(NpGameIntentInitParam), "param size mismatch");

    Require(Throws<std::invalid_argument>([] { sceNpGameIntentInfoInit(nullptr); }),
            "null info init did not throw");

    NpGameIntentInfo info{};
    Require(sceNpGameIntentInfoInit(&info) == 0, "info init failed");
    Require(info.size == sizeof(NpGameIntentInfo), "info size mismatch");
    Require(info.user_id == -1, "info user_id mismatch");

    Require(sceNpGameIntentInitialize(&initParam) == 0, "initialization failed");
    Require(sceNpGameIntentReceiveIntent(&info) == kIntentNotFound, "receive intent mismatch");

    char buf[64]{};
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", buf, sizeof(buf)) == kValueNotFound,
            "get property value mismatch");

    Require(sceNpGameIntentVshInitialize() == 0, "vsh init failed");
    Require(sceNpGameIntentVshTerminate() == 0, "vsh terminate failed");
    Require(sceNpGameIntentTerminate() == 0, "terminate failed");

    unsigned char appLauncherParam[0x28]{};
    *reinterpret_cast<std::uint64_t*>(appLauncherParam) = 0x28;
    Require(sceNpAppLauncherInitialize(appLauncherParam) == 0, "app launcher init failed");
    Require(Throws<std::runtime_error>([] { sceNpAppLauncherLaunchApp(); }),
            "app launcher launch must throw until implemented");
    Require(sceNpAppLauncherTerminate() == 0, "app launcher terminate failed");
    *reinterpret_cast<std::uint64_t*>(appLauncherParam) = 0x10;
    Require(sceNpAppLauncherInitialize(appLauncherParam) == 0,
            "app launcher init must accept any init parameter");

    std::puts("NpGameIntent tests passed");
    return 0;
}
