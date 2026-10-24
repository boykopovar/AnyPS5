#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceNpGameIntentInitParamInit(NpGameIntentInitParam* param);
int APS5_VABI sceNpGameIntentInfoInit(NpGameIntentInfo* info);
int APS5_VABI sceNpGameIntentInitialize(const void* initParam);
int APS5_VABI sceNpGameIntentReceiveIntent(NpGameIntentInfo* intentInfo);
int APS5_VABI sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intentData, const char* key, char* valueBuf, size_t bufSize);
int APS5_VABI sceNpGameIntentVshInitialize(void);
int APS5_VABI sceNpGameIntentVshTerminate(void);
int APS5_VABI sceNpGameIntentTerminate(void);
int APS5_VABI sceNpAppLauncherInitialize(const void* initParam);
int APS5_VABI sceNpAppLauncherLaunchApp(std::int32_t, const char*, const char*);
int APS5_VABI sceNpAppLauncherTerminate(void);
}

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
    constexpr int kInvalidArgument = -2141898748;
    constexpr int kNotInitialized = -2141898750;

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
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kNotInitialized,
            "launch before init must report not-initialized");
    Require(sceNpAppLauncherInitialize(appLauncherParam) == 0, "app launcher init failed");
    Require(sceNpAppLauncherLaunchApp(0, nullptr, nullptr) == kInvalidArgument,
            "null launch argument must be invalid");
    Require(sceNpAppLauncherLaunchApp(0, "", nullptr) == kInvalidArgument,
            "empty launch argument must be invalid");
    std::vector<char> unterminated(0x2001, 'x');
    Require(sceNpAppLauncherLaunchApp(0, "app", unterminated.data()) == kInvalidArgument,
            "non-terminated 0x2001-byte argument must be invalid");
    std::vector<char> bounded(0x2000, 'x');
    bounded.push_back('\0');
    Require(Throws<std::runtime_error>([&] { sceNpAppLauncherLaunchApp(0, "app", bounded.data()); }),
            "max-length argument launch must throw until implemented");
    Require(Throws<std::runtime_error>([] { sceNpAppLauncherLaunchApp(0, "app", nullptr); }),
            "app launcher launch must throw until implemented");
    Require(sceNpAppLauncherTerminate() == 0, "app launcher terminate failed");
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kNotInitialized,
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
    Require(sceNpAppLauncherLaunchApp(0, "app", nullptr) == kNotInitialized,
            "final launch must report not-initialized");

    std::puts("NpGameIntent tests passed");
    return 0;
}
