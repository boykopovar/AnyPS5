#include "prx/libSceSysUtil/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceRegMgr/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "SysUtil notification: %s\n", message);
    std::abort();
}

class StandardErrorCapture {
public:
    StandardErrorCapture() : file_(TemporaryFile::Open()) {
        Require(file_ != nullptr, "the capture file could not be opened");
        std::fflush(stderr);
#ifdef _WIN32
        saved_ = ::_dup(::_fileno(stderr));
        Require(saved_ >= 0 && ::_dup2(::_fileno(file_), ::_fileno(stderr)) == 0, "stderr could not be redirected");
#else
        saved_ = ::dup(::fileno(stderr));
        Require(saved_ >= 0 && ::dup2(::fileno(file_), ::fileno(stderr)) == ::fileno(stderr), "stderr could not be redirected");
#endif
    }

    ~StandardErrorCapture() {
        Restore();
        if (file_ != nullptr) std::fclose(file_);
    }

    std::string Text() {
        Restore();
        std::rewind(file_);
        std::string text;
        char buffer[256];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), file_)) text.append(buffer, count);
        return text;
    }

private:
    void Restore() {
        if (saved_ < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        ::_dup2(saved_, ::_fileno(stderr));
        ::_close(saved_);
#else
        ::dup2(saved_, ::fileno(stderr));
        ::close(saved_);
#endif
        saved_ = -1;
    }

    std::FILE* file_;
    int saved_ = -1;
};

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    std::string longText;
    for (int index = 0; index < 300; ++index) longText.push_back(static_cast<char>('a' + index % 26));

    int hello = -1;
    int empty = -1;
    int emptyString = -1;
    int type1 = -1;
    int type2 = -1;
    int truncated = -1;
    int generic = -1;
    std::string output;
    try {
        StandardErrorCapture capture;
        hello = sceSysUtilSendSystemNotificationWithText(7, "hello");
        empty = sceSysUtilSendSystemNotificationWithText(-1, nullptr);
        emptyString = sceSysUtilSendSystemNotificationWithText(0, "");
        type1 = sceSysUtilSendSystemNotificationWithText(1, "notification type 1");
        type2 = sceSysUtilSendSystemNotificationWithText(2, "notification type 2");
        truncated = sceSysUtilSendSystemNotificationWithText(0, longText.c_str());
        generic = sceSysUtilSendNotificationRequest("generic notification", 0);
        output = capture.Text();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SysUtil notification: %s\n", error.what());
        std::abort();
    }

    Require(hello == 0, "a text notification was not sent");
    Require(empty == 0, "a null message was not sent as an empty text");
    Require(emptyString == 0, "an empty string notification was not sent");
    Require(type1 == 0, "a type 1 notification was not sent");
    Require(type2 == 0, "a type 2 notification was not sent");
    Require(truncated == 0, "a long message was not sent");
    Require(generic == 0, "a generic notification request was not sent");
    errno = 71;
    Require(sceSysUtilSendSystemNotificationWithText(0, reinterpret_cast<const char*>(0x10)) == static_cast<int>(0x8002000Eu), "an unreadable message pointer did not return EFAULT");
    Require(errno == 71, "unreadable message pointer clobbered errno");
    errno = 71;
    Require(sceSysUtilSendSystemNotificationWithText(0, reinterpret_cast<const char*>(~std::uintptr_t{0xfff})) == static_cast<int>(0x8002000Eu), "an unreadable message pointer did not return EFAULT");
    Require(errno == 71, "high unreadable message pointer clobbered errno");
    const std::string expected =
        "[notification] hello\n"
        "[notification] \n"
        "[notification] \n"
        "[notification] notification type 1\n"
        "[notification] notification type 2\n"
        "[notification] " + longText.substr(0, 179) + "\n"
        "[notification] generic notification\n";
    if (output != expected) {
        std::fprintf(stderr, "notification output was:\n%s", output.c_str());
        Require(false, "the notification output differs from the sent texts");
    }

    Require(sceKernelSendNotificationRequest(0, reinterpret_cast<const void*>(0x10), 3120, 0) == static_cast<int>(0x8002000eu), "invalid request memory must return badAddress");
    std::vector<std::uint8_t> validReq(3120, 0);
    constexpr auto invalidArgument = static_cast<int>(0x80020016u);
    constexpr auto badAddress = static_cast<int>(0x8002000eu);
    constexpr auto notSupported = static_cast<int>(0x8002002du);

    Require(sceKernelSendNotificationRequest(0, nullptr, 3120, 0) == badAddress, "null request must return badAddress");
    Require(sceKernelSendNotificationRequest(-1, validReq.data(), 3120, 0) == invalidArgument, "negative device must return invalidArgument");
    Require(sceKernelSendNotificationRequest(0, validReq.data(), 0, 0) == invalidArgument, "zero size must return invalidArgument");
    Require(sceKernelSendNotificationRequest(0, validReq.data(), 3119, 0) == invalidArgument, "undersized request must return invalidArgument");
    Require(sceKernelSendNotificationRequest(0, validReq.data(), 3121, 0) == invalidArgument, "oversized request must return invalidArgument");
    Require(sceKernelSendNotificationRequest(1, validReq.data(), 3120, 0) == notSupported, "nonzero device must return notSupported");
    Require(sceKernelSendNotificationRequest(0, validReq.data(), 3120, 1) == notSupported, "nonzero flags must return notSupported");
    Require(sceSysUtilSendSystemNotification(1) == notSupported, "kind 1 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithUserId(1, 42) == notSupported, "kind 2 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithDeviceId(1, nullptr) == notSupported, "kind 3 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithDeviceIdRelatedToUser(1, 42, nullptr) == notSupported, "kind 4 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithTextRelatedToUser(1, 42, "message") == notSupported, "kind 6 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithErrorCode(1, -1) == notSupported, "kind 7 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithAppId(1, 42) == notSupported, "kind 8 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithAppName(1, "app") == notSupported, "kind 9 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithAppInfo(1, "app", "info") == notSupported, "app info must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithAppNameRelatedToUser(1, 42, "app") == notSupported, "kind 10 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithUserName(1, "user") == notSupported, "kind 12 must reach the unsupported host notification handler");
    Require(sceSysUtilSendSystemNotificationWithUserNameInfo(1, "user", "info") == notSupported, "kind 13 must reach the unsupported host notification handler");
    Require(sceSysUtilSendAddressingSystemNotification(1, 2) == notSupported, "kind 14 must reach the unsupported host notification handler");
    Require(sceSysUtilSendAddressingSystemNotificationWithDeviceId(1, 2, nullptr) == notSupported, "kind 15 must reach the unsupported host notification handler");
    Require(sceSysUtilSendAddressingSystemNotificationWithUserName(1, 2, "user") == notSupported, "kind 16 must reach the unsupported host notification handler");
    Require(sceSysUtilSendAddressingSystemNotificationWithUserId(1, 2, 3) == notSupported, "kind 17 must reach the unsupported host notification handler");
    Require(sceSysUtilSendNotificationRequest("generic notification", 1) == notSupported, "generic notification flags must reach the kernel validation");
    constexpr std::int64_t debugSetting = 0x0d49671adfd5eb34;
    constexpr std::int64_t webDebugSetting = 0x1c0b63438bdc7b40;
    Require(sceRegMgrNonSysSetInt(debugSetting, 0, 0) == 0, "the debug-notification setting could not be disabled");
    Require(sceRegMgrNonSysSetInt(webDebugSetting, 0, 0) == 0, "the web-debug setting could not be disabled");
    Require(sceSysUtilSendTrcCheckNotificationRequest("disabled", 0) == 0, "a disabled TRC notification did not return success");
    Require(sceSysUtilSendNpDebugNotificationRequest("disabled", 0) == 0, "a disabled NP debug notification did not return success");
    Require(sceSysUtilSendWebDebugNotificationRequest("disabled", 0) == 0, "a disabled web debug notification did not return success");
    Require(sceRegMgrNonSysSetInt(debugSetting, 0, 1) == 0, "the debug-notification setting could not be enabled");
    Require(sceRegMgrNonSysSetInt(webDebugSetting, 0, 1) == 0, "the web-debug setting could not be enabled");
    Require(sceSysUtilSendTrcCheckNotificationRequest("enabled", 0) == notSupported, "enabled TRC notification did not reach the kernel handler");
    Require(sceSysUtilSendNpDebugNotificationRequest("enabled", 0) == notSupported, "enabled NP debug notification did not reach the kernel handler");
    Require(sceSysUtilSendWebDebugNotificationRequest("enabled", 0) == notSupported, "enabled web debug notification did not reach the kernel handler");
    errno = 71;
    Require(sceSysUtilSendNpDebugNotificationRequest(reinterpret_cast<const char*>(0x10), 0) == static_cast<int>(0x8002000Eu), "an unreadable enabled debug message did not return EFAULT");
    Require(errno == 71, "unreadable enabled debug message clobbered errno");
    std::vector<std::uint8_t> initializedParams(0xa40, 0xaa);
    sceSysUtilSendSystemNotificationParamInit(initializedParams.data(), 3);
    std::uint32_t paramType = 0;
    std::uint32_t paramSentinel = 0;
    std::uint64_t secondParamSentinel = 0;
    std::uint32_t paramTag = 0;
    std::memcpy(&paramType, initializedParams.data(), sizeof(paramType));
    std::memcpy(&paramSentinel, initializedParams.data() + 0x818, sizeof(paramSentinel));
    std::memcpy(&secondParamSentinel, initializedParams.data() + 0x820, sizeof(secondParamSentinel));
    std::memcpy(&paramTag, initializedParams.data() + 0xa38, sizeof(paramTag));
    Require(paramType == 3 && paramSentinel == 0xffffffffu && secondParamSentinel == ~std::uint64_t{0} && paramTag == 0xd59c4b6e, "parameter initialization fields do not match the firmware layout");
    sceSysUtilSendSystemNotificationParamInit(nullptr, 3);
    Require(sceSysUtilSendSystemNotification2(nullptr) == static_cast<int>(0x81670000u), "a null notification parameter block did not return the firmware error");
    int initializedTextResult = -1;
    std::string initializedTextOutput;
    {
        StandardErrorCapture capture;
        sceSysUtilSendSystemNotificationParamInit(initializedParams.data(), 4);
        initializedParams[0x818] = 0;
        initializedTextResult = sceSysUtilSendSystemNotification2(initializedParams.data());
        initializedTextOutput = capture.Text();
    }
    Require(initializedTextResult == 0 && initializedTextOutput == "[notification] \n", "parameter type 4 did not send its initialized text request");
    for (std::int32_t type = 0; type <= 13; ++type) {
        if (type == 4) continue;
        sceSysUtilSendSystemNotificationParamInit(initializedParams.data(), type);
        Require(sceSysUtilSendSystemNotification2(initializedParams.data()) == notSupported, "a valid parameter type did not dispatch to the kernel notification handler");
    }
    sceSysUtilSendSystemNotificationParamInit(initializedParams.data(), 14);
    std::memcpy(&paramType, initializedParams.data(), sizeof(paramType));
    std::memcpy(&paramTag, initializedParams.data() + 0xa38, sizeof(paramTag));
    Require(paramType == 14 && paramTag == 0xd59c4b6e, "out-of-range parameter types must still receive the initialized tag");
    Require(sceSysUtilSendSystemNotification2(initializedParams.data()) == static_cast<int>(0x81670000u), "an out-of-range parameter type did not return the firmware error");
    std::memset(initializedParams.data() + 0xa38, 0, sizeof(paramTag));
    Require(sceSysUtilSendSystemNotification2(initializedParams.data()) == static_cast<int>(0x81670000u), "an uninitialized notification parameter block did not return the firmware error");
    errno = 71;
    Require(sceSysUtilSendSystemNotification2(reinterpret_cast<const void*>(0x10)) == static_cast<int>(0x8002000Eu), "an unreadable notification parameter block did not return EFAULT");
    Require(errno == 71, "unreadable notification parameter block clobbered errno");
    errno = 71;
    Require(sceSysUtilSendSystemNotificationWithTextRelatedToUser(1, 42, reinterpret_cast<const char*>(0x10)) == static_cast<int>(0x8002000Eu), "an unreadable related message pointer did not return EFAULT");
    Require(errno == 71, "unreadable related message pointer clobbered errno");
    errno = 71;
    Require(sceSysUtilSendNotificationRequest(reinterpret_cast<const char*>(0x10), 0) == static_cast<int>(0x8002000Eu), "an unreadable generic message pointer did not return EFAULT");
    Require(errno == 71, "unreadable generic message pointer clobbered errno");
    std::puts("SysUtil notification tests passed");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
