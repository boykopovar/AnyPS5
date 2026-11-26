#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Constants.hpp"

namespace {

void require(bool condition, const char* expr) {
    if (!condition) {
        std::fprintf(stderr, "SystemExports assertion failed: %s\n", expr);
        std::abort();
    }
}

struct TestNpCommId {
    char data[9];
    char term;
    std::uint8_t num;
    char dummy;
};
static_assert(sizeof(TestNpCommId) == 12);

constexpr int kErrInvalidArgument = static_cast<int>(0x80553904u);
constexpr int kErrBufferTooSmall = static_cast<int>(0x80553905u);
constexpr int kErrInvalidHandle = static_cast<int>(0x80553908u);
constexpr int kErrInvalidContext = static_cast<int>(0x80553909u);
constexpr int kErrInvalidTrophyId = static_cast<int>(0x8055390au);
constexpr int kErrInvalidGroupId = static_cast<int>(0x8055390bu);
constexpr int kErrInvalidUserId = static_cast<int>(0x8055391du);

void testSystemHandles() {
    require(sceNpTrophy2SystemCreateHandle(nullptr) == kErrInvalidArgument, "CreateHandle nullptr");
    require(sceNpTrophy2SystemAbortHandle(-1) == kErrInvalidHandle, "AbortHandle -1");
    require(sceNpTrophy2SystemDestroyHandle(-1) == kErrInvalidHandle, "DestroyHandle -1");
    require(sceNpTrophy2SystemAbortHandle(9999) == kErrInvalidHandle, "AbortHandle unknown");
    require(sceNpTrophy2SystemDestroyHandle(9999) == kErrInvalidHandle, "DestroyHandle unknown");

    int h1 = 0xA5A5A5A5;
    int h2 = 0xA5A5A5A5;
    require(sceNpTrophy2SystemCreateHandle(&h1) == SCE_NP_TROPHY2_OK, "CreateHandle h1");
    require(sceNpTrophy2SystemCreateHandle(&h2) == SCE_NP_TROPHY2_OK, "CreateHandle h2");
    require(h1 > 0 && h2 > h1, "handles positive monotonic");
    require(sceNpTrophy2SystemAbortHandle(h1) == SCE_NP_TROPHY2_OK, "AbortHandle h1");
    require(sceNpTrophy2SystemDestroyHandle(h1) == SCE_NP_TROPHY2_OK, "DestroyHandle h1");
    require(sceNpTrophy2SystemAbortHandle(h1) == kErrInvalidHandle, "AbortHandle destroyed h1");
    require(sceNpTrophy2SystemDestroyHandle(h1) == kErrInvalidHandle, "DestroyHandle destroyed h1");
    require(sceNpTrophy2SystemDestroyHandle(h2) == SCE_NP_TROPHY2_OK, "DestroyHandle h2");
}

void testSystemContextAndCallbacks() {
    TestNpCommId comm{{'N', 'P', 'W', 'R', '1', '2', '3', '4', '5'}, '\0', 0, '\0'};
    int ctx = 0xA5A5A5A5;

    require(sceNpTrophy2SystemCreateContext(&ctx, -1, &comm, 0) == kErrInvalidUserId, "CreateContext invalid user");
    require(sceNpTrophy2SystemCreateContext(nullptr, 1, &comm, 0) == kErrInvalidArgument, "CreateContext null ctx");
    require(sceNpTrophy2SystemCreateContext(&ctx, 1, nullptr, 0) == kErrInvalidArgument, "CreateContext null comm");
    require(sceNpTrophy2SystemCreateContext(&ctx, 1, &comm, 1) == kErrInvalidArgument, "CreateContext bad options");
    require(sceNpTrophy2SystemDestroyContext(-1) == kErrInvalidContext, "DestroyContext -1");
    require(sceNpTrophy2SystemDestroyContext(9999) == kErrInvalidContext, "DestroyContext unknown");

    require(sceNpTrophy2SystemCreateContext(&ctx, 1, &comm, 0) == SCE_NP_TROPHY2_OK, "CreateContext valid");
    require(ctx > 0, "ctx positive");
    require(sceNpTrophy2SystemDestroyContext(ctx) == SCE_NP_TROPHY2_OK, "DestroyContext valid");
    require(sceNpTrophy2SystemDestroyContext(ctx) == kErrInvalidContext, "DestroyContext double");

    require(sceNpTrophy2SystemCheckCallback() == SCE_NP_TROPHY2_OK, "CheckCallback");
    require(sceNpTrophy2SystemRegisterTitleUpdateCallback(nullptr, nullptr) == kErrInvalidArgument, "RegisterUpdate null");
    require(sceNpTrophy2SystemRegisterTitleSyncedCallback(nullptr, nullptr) == kErrInvalidArgument, "RegisterSynced null");
    int marker = 1;
    require(sceNpTrophy2SystemRegisterTitleUpdateCallback(reinterpret_cast<void*>(&testSystemHandles), &marker) == SCE_NP_TROPHY2_OK, "RegisterUpdate valid");
    require(sceNpTrophy2SystemRegisterTitleSyncedCallback(reinterpret_cast<void*>(&testSystemHandles), &marker) == SCE_NP_TROPHY2_OK, "RegisterSynced valid");
    require(sceNpTrophy2SystemUnregisterTitleUpdateCallback() == SCE_NP_TROPHY2_OK, "UnregisterUpdate");
    require(sceNpTrophy2SystemUnregisterTitleSyncedCallback() == SCE_NP_TROPHY2_OK, "UnregisterSynced");

    require(sceNpTrophy2SystemIsServerAvailable(nullptr) == kErrInvalidArgument, "IsServerAvailable null");
    std::uint8_t availByte = 0xA5;
    bool avail = false;
    std::memcpy(&avail, &availByte, sizeof(avail));
    require(sceNpTrophy2SystemIsServerAvailable(&avail) == SCE_NP_TROPHY2_OK, "IsServerAvailable valid");
    require(!avail, "avail false");
}

void testSystemIconUriBuilders() {
    TestNpCommId empty{};
    TestNpCommId comm{{'N', 'P', 'W', 'R', '1', '2', '3', '4', '5'}, '\0', 7, '\0'};
    char buf[128];

    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildTitleIconUri(nullptr, buf, sizeof(buf)) == kErrInvalidArgument, "BuildTitleIconUri null comm");
    require(sceNpTrophy2SystemBuildTitleIconUri(&empty, buf, sizeof(buf)) == kErrInvalidArgument, "BuildTitleIconUri empty comm");
    require(sceNpTrophy2SystemBuildTitleIconUri(&comm, nullptr, sizeof(buf)) == kErrBufferTooSmall, "BuildTitleIconUri null buf");
    require(sceNpTrophy2SystemBuildTitleIconUri(&comm, buf, 34) == kErrBufferTooSmall, "BuildTitleIconUri small buf");
    require(sceNpTrophy2SystemBuildTitleIconUri(&comm, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildTitleIconUri valid");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/icon") == 0, "BuildTitleIconUri text");

    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, std::numeric_limits<int>::min(), buf, sizeof(buf)) == kErrInvalidGroupId, "BuildGroupIconUri INT_MIN");
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, -2, buf, sizeof(buf)) == kErrInvalidGroupId, "BuildGroupIconUri -2");
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, 0, buf, sizeof(buf)) == kErrInvalidGroupId, "BuildGroupIconUri 0");
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, 51, buf, sizeof(buf)) == kErrInvalidGroupId, "BuildGroupIconUri 51");
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, std::numeric_limits<int>::max(), buf, sizeof(buf)) == kErrInvalidGroupId, "BuildGroupIconUri INT_MAX");
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, -1, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildGroupIconUri -1");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/trophyGroups/-1/icon") == 0, "BuildGroupIconUri -1 text");
    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, 1, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildGroupIconUri 1");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/trophyGroups/1/icon") == 0, "BuildGroupIconUri 1 text");
    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildGroupIconUri(&comm, 50, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildGroupIconUri 50");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/trophyGroups/50/icon") == 0, "BuildGroupIconUri 50 text");

    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildTrophyIconUri(&comm, -1, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildTrophyIconUri -1");
    require(sceNpTrophy2SystemBuildTrophyIconUri(&comm, 1000, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildTrophyIconUri 1000");
    require(sceNpTrophy2SystemBuildTrophyIconUri(&comm, 999, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildTrophyIconUri 999");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/trophies/999/icon") == 0, "BuildTrophyIconUri 999 text");

    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildRewardIconUri(&comm, -1, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildRewardIconUri -1");
    require(sceNpTrophy2SystemBuildRewardIconUri(&comm, 1000, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildRewardIconUri 1000");
    require(sceNpTrophy2SystemBuildRewardIconUri(&comm, 12, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildRewardIconUri 12");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/rewards/12/icon") == 0, "BuildRewardIconUri 12 text");

    std::memset(buf, 0xA5, sizeof(buf));
    require(sceNpTrophy2SystemBuildBgImageIconUri(&comm, -1, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildBgImageIconUri -1");
    require(sceNpTrophy2SystemBuildBgImageIconUri(&comm, 1000, buf, sizeof(buf)) == kErrInvalidTrophyId, "BuildBgImageIconUri 1000");
    require(sceNpTrophy2SystemBuildBgImageIconUri(&comm, 3, buf, sizeof(buf)) == SCE_NP_TROPHY2_OK, "BuildBgImageIconUri 3");
    require(std::strcmp(buf, "trp2://localfile/NPWR12345_07/bgImages/3/icon") == 0, "BuildBgImageIconUri 3 text");
}

}

int main() {
    testSystemHandles();
    testSystemContextAndCallbacks();
    testSystemIconUriBuilders();
    return 0;
}
