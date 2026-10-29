#include "prx/libSceNpGameIntent/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

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
    constexpr int kNotInitialized = -2141898750;
    constexpr int kInvalidArgument = -2141898748;
    constexpr int kIntentNotFound = -2141898746;
    constexpr int kValueNotFound = -2141898745;
    constexpr int kLauncherInvalidArgument = -2141898236;
    constexpr int kLauncherNotInitialized = -2141898238;

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

    char preBuf[64]{};
    Require(sceNpGameIntentGetPropertyValueString(nullptr, "key", preBuf, sizeof(preBuf)) == kNotInitialized,
            "uninitialized get property string with null data must report not-initialized");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", preBuf, sizeof(preBuf)) == kNotInitialized,
            "uninitialized get property string with valid data must report not-initialized");
    Require(sceNpGameIntentReceiveIntent(nullptr) == kNotInitialized,
            "uninitialized receive intent with null pointer must report not-initialized");
    Require(sceNpGameIntentReceiveIntent(&info) == kNotInitialized,
            "uninitialized receive intent with valid pointer must report not-initialized");

    Require(sceNpGameIntentInitialize(&initParam) == 0, "initialization failed");
    Require(sceNpGameIntentReceiveIntent(nullptr) == kInvalidArgument, "null receive intent did not fail");
    Require(sceNpGameIntentReceiveIntent(&info) == kInvalidArgument, "mismatched struct size must report invalid argument");
    info.size = 0;
    Require(sceNpGameIntentReceiveIntent(&info) == kInvalidArgument, "zero size must report invalid argument");
    info.size = 0x413f;
    Require(sceNpGameIntentReceiveIntent(&info) == kInvalidArgument, "0x413f size must report invalid argument");
    info.size = 0x4141;
    Require(sceNpGameIntentReceiveIntent(&info) == kInvalidArgument, "0x4141 size must report invalid argument");
    info.size = sizeof(NpGameIntentInfo);

    char buf[64]{};
    Require(sceNpGameIntentGetPropertyValueString(nullptr, "key", buf, sizeof(buf)) == kInvalidArgument,
            "null intent data did not fail");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, nullptr, buf, sizeof(buf)) == kInvalidArgument,
            "null key did not fail");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "", buf, sizeof(buf)) == kInvalidArgument,
            "empty key did not fail");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", nullptr, sizeof(buf)) == kInvalidArgument,
            "null value buf did not fail");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", buf, 0) == kInvalidArgument,
            "zero buf size did not fail");

    char canaryBuf[64]{};
    std::memset(canaryBuf, 'X', sizeof(canaryBuf));
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", canaryBuf, 0) == kInvalidArgument,
            "zero buf size canary call did not fail");
    Require(canaryBuf[0] == 'X', "buffer was modified on zero size error");
    Require(sceNpGameIntentGetPropertyValueString(nullptr, "key", canaryBuf, sizeof(canaryBuf)) == kInvalidArgument,
            "null data canary call did not fail");
    Require(canaryBuf[0] == 'X', "buffer was modified on null data error");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, nullptr, canaryBuf, sizeof(canaryBuf)) == kInvalidArgument,
            "null key canary call did not fail");
    Require(canaryBuf[0] == 'X', "buffer was modified on null key error");
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "", canaryBuf, sizeof(canaryBuf)) == kInvalidArgument,
            "empty key canary call did not fail");
    Require(canaryBuf[0] == 'X', "buffer was modified on empty key error");

    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", canaryBuf, sizeof(canaryBuf)) == kValueNotFound,
            "get property value mismatch");
    Require(canaryBuf[0] == '\0', "valid call did not write terminator");

    canaryBuf[0] = 'Y';
    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", canaryBuf, 1) == kValueNotFound,
            "bufSize 1 did not succeed");
    Require(canaryBuf[0] == '\0', "bufSize 1 did not write terminator");

    Require(sceNpGameIntentVshInitialize() == 0, "vsh init failed");
    Require(sceNpGameIntentVshTerminate() == 0, "vsh terminate failed");
    Require(sceNpGameIntentTerminate() == 0, "terminate failed");

    Require(sceNpGameIntentGetPropertyValueString(&info.intent_data, "key", buf, sizeof(buf)) == kNotInitialized,
            "post-terminate get property value must report not-initialized");
    Require(sceNpGameIntentReceiveIntent(&info) == kNotInitialized,
            "post-terminate receive intent must report not-initialized");

    unsigned char appLauncherParam[0x28]{};
    *reinterpret_cast<std::uint64_t*>(appLauncherParam) = 0x28;
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kLauncherNotInitialized,
            "launch before init must report not-initialized");
    Require(sceNpAppLauncherInitialize(appLauncherParam) == 0, "app launcher init failed");
    Require(sceNpAppLauncherLaunchApp(0, nullptr, nullptr) == kLauncherInvalidArgument,
            "null launch argument must be invalid");
    Require(sceNpAppLauncherLaunchApp(0, "", nullptr) == kLauncherInvalidArgument,
            "empty launch argument must be invalid");
    std::vector<char> unterminated(0x2001, 'x');
    Require(sceNpAppLauncherLaunchApp(0, "app", unterminated.data()) == kLauncherInvalidArgument,
            "non-terminated 0x2001-byte argument must be invalid");
    Require(sceNpAppLauncherTerminate() == 0, "app launcher terminate failed");
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kLauncherNotInitialized,
            "launch after terminate must report not-initialized");
    *reinterpret_cast<std::uint64_t*>(appLauncherParam) = 0x10;
    Require(sceNpAppLauncherInitialize(appLauncherParam) == 0,
            "app launcher init must accept any init parameter");

    std::atomic_bool stop{false};
    std::thread writer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            sceNpAppLauncherInitialize(nullptr);
            sceNpAppLauncherTerminate();
        }
    });
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            try {
                (void)sceNpAppLauncherLaunchApp(0, "app", nullptr);
            } catch (const std::runtime_error&) {
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.store(true);
    writer.join();
    reader.join();

    Require(sceNpAppLauncherTerminate() == 0, "final terminate failed");
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kLauncherNotInitialized,
            "final launch must report not-initialized");

    std::puts("NpGameIntent tests passed");
    return 0;
}
