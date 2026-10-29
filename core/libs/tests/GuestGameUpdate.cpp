#include "prx/libSceGameUpdate/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
static void RequireThrows(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

static void TestRequestArguments(int notInitialized, int invalidArgument, int invalidSize, int notFound) {
    GameUpdateCheckParam param{};
    param.size = sizeof(param);
    GameUpdateCheckResult result{};
    result.size = sizeof(result);
    constexpr int requestId = 0x20000000;

    for (int id : {0, -1}) {
        Require(sceGameUpdateCheck(id, nullptr, nullptr) == notFound);
        Require(sceGameUpdateAbortRequest(id) == notFound);
        Require(sceGameUpdateDeleteRequest(id) == notFound);
    }
    Require(sceGameUpdateCheck(requestId, nullptr, &result) == invalidArgument);
    Require(sceGameUpdateCheck(requestId, &param, nullptr) == invalidArgument);
    param.size = sizeof(param) - 1;
    Require(sceGameUpdateCheck(requestId, &param, &result) == invalidSize);
    param.size = sizeof(param);
    result.size = sizeof(result) + 8;
    Require(sceGameUpdateCheck(requestId, &param, &result) == invalidSize);
    result.size = sizeof(result);
    param.option = 1;
    Require(sceGameUpdateCheck(requestId, &param, &result) == invalidArgument);
    param.option = 0;
    param.reserved[8] = 1;
    Require(sceGameUpdateCheck(requestId, &param, &result) == invalidArgument);
    param.reserved[8] = 0;

    Require(sceGameUpdateCreateRequest() == notInitialized);
    Require(sceGameUpdateCheck(requestId, &param, &result) == notInitialized);
    Require(sceGameUpdateAbortRequest(requestId) == notInitialized);
    Require(sceGameUpdateDeleteRequest(requestId) == notInitialized);

    Require(sceGameUpdateInitialize() == 0);
    Require(sceGameUpdateCheck(requestId, &param, &result) == notFound);
    Require(sceGameUpdateAbortRequest(requestId) == notFound);
    Require(sceGameUpdateDeleteRequest(requestId) == notFound);
    Require(sceGameUpdateTerminate() == 0);
}

static void TestRequestLifecycle(int notFound) {
    constexpr int tooManyRequests = static_cast<int>(0x80412806);
    constexpr int aborted = static_cast<int>(0x80412807);
    GameUpdateCheckParam param{};
    param.size = sizeof(param);
    GameUpdateCheckResult result{};
    std::memset(&result, 0xAB, sizeof(result));
    result.size = sizeof(result);
    const auto original = result;

    int ids[8];
    for (int& id : ids) {
        id = sceGameUpdateCreateRequest();
        Require(id >= 0x20000000 && id <= 0x2FFFFFFF);
    }
    Require(ids[0] != ids[1]);
    Require(sceGameUpdateCreateRequest() == tooManyRequests);

    Require(sceGameUpdateCheck(ids[0], &param, &result) == 0);
    Require(!result.found && !result.addcont_found);
    Require(std::memcmp(&result, &original, 8) == 0);
    Require(std::memcmp(reinterpret_cast<const char*>(&result) + 10, reinterpret_cast<const char*>(&original) + 10, sizeof(result) - 10) == 0);

    Require(sceGameUpdateAbortRequest(ids[1]) == 0);
    Require(sceGameUpdateCheck(ids[1], &param, &result) == aborted);

    Require(sceGameUpdateDeleteRequest(ids[3]) == 0);
    Require(sceGameUpdateDeleteRequest(ids[3]) == notFound);
    Require(sceGameUpdateCheck(ids[3], &param, &result) == notFound);
    Require(sceGameUpdateCreateRequest() >= 0x20000000);

    Require(sceGameUpdateTerminate() == 0);
    Require(sceGameUpdateInitialize() == 0);
    Require(sceGameUpdateCheck(ids[0], &param, &result) == notFound);
}

int main() {
    constexpr int notInitialized = static_cast<int>(0x80412801);
    constexpr int invalidArgument = static_cast<int>(0x80412803);
    constexpr int invalidSize = static_cast<int>(0x80412804);

    static_assert(sizeof(GameUpdateAddcontVersionInfo) == 0x30);
    const NpUnifiedEntitlementLabel label{};
    GameUpdateAddcontVersionInfo info{};
    info.size = sizeof(info);

    constexpr int alreadyInitialized = static_cast<int>(0x80412802);
    constexpr int notFound = static_cast<int>(0x80412805);

    Require(sceGameUpdateTerminate() == notInitialized);
    TestRequestArguments(notInitialized, invalidArgument, invalidSize, notFound);

    Require(sceGameUpdateGetAddcontLatestVersion(0, &label, &info) == notInitialized);
    Require(sceGameUpdateGetAddcontLatestVersion(0, &label, nullptr) == invalidArgument);
    Require(sceGameUpdateGetAddcontLatestVersion(0, nullptr, &info) == invalidArgument);
    Require(sceGameUpdateGetAddcontLatestVersion(0, nullptr, nullptr) == invalidArgument);
    Require(sceGameUpdateInitialize() == 0);
    Require(sceGameUpdateInitialize() == alreadyInitialized);
    Require(sceGameUpdateGetAddcontLatestVersion(0, &label, nullptr) == invalidArgument);

    Require(sceGameUpdateGetAddcontLatestVersion(0, nullptr, &info) == invalidArgument);

    info.size = sizeof(info) - 1;
    RequireThrows([&] { sceGameUpdateGetAddcontLatestVersion(0, &label, &info); });

    std::memset(&info, 0xAB, sizeof(info));
    info.size = sizeof(info) + 8;
    const auto original = info;
    RequireThrows([&] { sceGameUpdateGetAddcontLatestVersion(0, &label, &info); });
    Require(std::memcmp(&info, &original, sizeof(info)) == 0);
    TestRequestLifecycle(notFound);

    Require(sceGameUpdateTerminate() == 0);
    Require(sceGameUpdateTerminate() == notInitialized);
    info.size = sizeof(info);
    Require(sceGameUpdateGetAddcontLatestVersion(0, &label, &info) == notInitialized);
    Require(sceGameUpdateGetAddcontLatestVersion(0, &label, nullptr) == invalidArgument);
    Require(sceGameUpdateGetAddcontLatestVersion(0, nullptr, &info) == invalidArgument);
    Require(sceGameUpdateGetAddcontLatestVersion(0, nullptr, nullptr) == invalidArgument);
    Require(sceGameUpdateCreateRequest() == notInitialized);
    GameUpdateCheckParam postParam{};
    postParam.size = sizeof(postParam);
    GameUpdateCheckResult postResult{};
    postResult.size = sizeof(postResult);
    Require(sceGameUpdateCheck(0x20000000, &postParam, &postResult) == notInitialized);
    Require(sceGameUpdateAbortRequest(0x20000000) == notInitialized);
    Require(sceGameUpdateDeleteRequest(0x20000000) == notInitialized);
}
