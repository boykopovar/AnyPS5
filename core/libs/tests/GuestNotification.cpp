#ifndef _WIN32
#include "prx/libc/include/general/LogMacros.hpp"
#endif
#include "prx/libSceNotification/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "tests/TemporaryFile.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NOTIFICATION: %s\n", message);
        std::abort();
    }
}

std::uint8_t* Pointer(std::string& value) {
    return reinterpret_cast<std::uint8_t*>(value.data());
}

#ifndef _WIN32
std::string BannerLog;

int CaptureBannerLine(std::FILE*, const char* text, std::size_t length) {
    BannerLog.append(text, length);
    return 0;
}
#endif

void TestSend() {
    std::string message = "Hello from the guest test";
    Require(sceNotificationSend(0, 0, Pointer(message)) == 0, "send a notification");
    std::string typed = "typed";
    Require(sceNotificationSend(1, 0x5a, Pointer(typed)) == 0, "send a notification of another type and flag");
    std::string longest(Notification::MessageCapacity, 'a');
    Require(sceNotificationSend(0, 0, Pointer(longest)) == 0, "send a message of the full 0x1800 bytes");
    std::string overlong(Notification::MessageCapacity + 1, 'b');
    Require(sceNotificationSend(0, 0, Pointer(overlong)) == Notification::InvalidMessage,
            "send a message over the 0x1800-byte limit");
    std::string empty;
    Require(sceNotificationSend(0, 0, Pointer(empty)) == Notification::InvalidMessage, "send an empty message");
    Require(sceNotificationSend(0, 0, nullptr) == Notification::InvalidMessage, "send a null message");
    errno = 71;
    Require(sceNotificationSend(0, 0, reinterpret_cast<std::uint8_t*>(0x10)) == Notification::InvalidMessage,
            "send an unreadable message");
    Require(errno == 71, "unreadable message changed host errno");
}

void TestSendById() {
    std::string id = "PPSA99000";
    std::string json = R"({"text":"Installed"})";
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(json)) == 0, "send a notification by id");
    std::string longestId(Notification::IdCapacity, 'c');
    std::string longestJson(Notification::MessageCapacity, 'd');
    Require(sceNotificationSendById(0, 0, Pointer(longestId), Pointer(longestJson)) == 0,
            "send by id with a full-size id and body");
    std::string overlongId(Notification::IdCapacity + 1, 'e');
    Require(sceNotificationSendById(0, 0, Pointer(overlongId), Pointer(json)) == Notification::InvalidMessage,
            "send by id with an id over the 0x20-byte limit");
    std::string overlongJson(Notification::MessageCapacity + 1, 'f');
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(overlongJson)) == Notification::InvalidMessage,
            "send by id with a body over the 0x1800-byte limit");
    std::string emptyId;
    std::string emptyJson;
    Require(sceNotificationSendById(0, 0, Pointer(emptyId), Pointer(json)) == Notification::InvalidMessage,
            "send by id with an empty id");
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(emptyJson)) == Notification::InvalidMessage,
            "send by id with an empty body");
    Require(sceNotificationSendById(0, 0, nullptr, Pointer(json)) == Notification::InvalidMessage,
            "send by id with a null id");
    Require(sceNotificationSendById(0, 0, Pointer(id), nullptr) == Notification::InvalidMessage,
            "send by id with a null body");
    errno = 71;
    Require(sceNotificationSendById(0, 0, reinterpret_cast<std::uint8_t*>(0x10), Pointer(json)) == Notification::InvalidMessage,
            "send by id with an unreadable id");
    Require(sceNotificationSendById(0, 0, Pointer(id), reinterpret_cast<std::uint8_t*>(0x10)) == Notification::InvalidMessage,
            "send by id with an unreadable body");
    Require(errno == 71, "unreadable id or body changed host errno");
}

void TestBanner() {
    std::string config = R"({"text":"Update ready"})";
    Require(sceNotificationShowPsButtonPersistentBanner(Pointer(config)) == 0, "show the persistent banner");
    Require(sceNotificationShowPsButtonPersistentBanner(nullptr) == 0,
            "show the persistent banner with a null configuration");
    errno = 71;
    Require(sceNotificationShowPsButtonPersistentBanner(reinterpret_cast<std::uint8_t*>(0x10)) == Notification::InvalidMessage,
            "show the persistent banner with an unreadable configuration");
    Require(errno == 71, "unreadable banner configuration changed host errno");

#ifndef _WIN32
    BannerLog.clear();
    LogSink().store(&CaptureBannerLine, std::memory_order_release);
    sceNotificationShowPsButtonPersistentBanner(nullptr);
    sceNotificationShowPsButtonPersistentBanner(Pointer(config));
    LogSink().store(nullptr, std::memory_order_release);
    Require(BannerLog.find("banner={}") != std::string::npos, "a null banner configuration logs the default object");
    Require(BannerLog.find(config) != std::string::npos, "a non-null banner configuration is logged verbatim");
#endif
}

void TestHide() {
    sceNotificationHidePsButtonPersistentBanner();
}

void TestKernelNotificationErrorPaths() {
    constexpr auto invalidArgument = static_cast<int>(0x80020016u);
    constexpr auto badAddress = static_cast<int>(0x8002000eu);
    constexpr auto notSupported = static_cast<int>(0x8002002du);

    std::vector<std::uint8_t> request(3120, 0);
    Require(sceKernelSendNotificationRequest(0, request.data(), 0, 0) == invalidArgument,
            "zero-size kernel notification must fail with EINVAL");
    Require(sceKernelSendNotificationRequest(0, request.data(), 3119, 0) == invalidArgument,
            "undersized kernel notification must fail with EINVAL");
    Require(sceKernelSendNotificationRequest(0, request.data(), 3121, 0) == invalidArgument,
            "oversized kernel notification must fail with EINVAL");
    Require(sceKernelSendNotificationRequest(-1, request.data(), request.size(), 0) == invalidArgument,
            "negative device must fail with EINVAL");
    Require(sceKernelSendNotificationRequest(0, nullptr, request.size(), 0) == badAddress,
            "null request must fail with EFAULT");
    errno = 71;
    Require(sceKernelSendNotificationRequest(0, reinterpret_cast<const void*>(0x10), request.size(), 0) == badAddress,
            "unreadable request must fail with EFAULT");
    Require(errno == 71, "unreadable kernel notification changed host errno");
    Require(sceKernelSendNotificationRequest(1, request.data(), request.size(), 0) == notSupported,
            "nonzero device must fail with ENOTSUP");
    Require(sceKernelSendNotificationRequest(0, request.data(), request.size(), 1) == notSupported,
            "nonzero flags must fail with ENOTSUP");

    std::vector<std::uint8_t> badType = request;
    const std::int32_t unsupportedType = 1;
    std::memcpy(badType.data(), &unsupportedType, sizeof(unsupportedType));
    Require(sceKernelSendNotificationRequest(0, badType.data(), badType.size(), 0) == notSupported,
            "unsupported request type must fail with ENOTSUP");

    std::vector<std::uint8_t> dirtySentinel = request;
    dirtySentinel[44] = 1;
    Require(sceKernelSendNotificationRequest(0, dirtySentinel.data(), dirtySentinel.size(), 0) == notSupported,
            "nonzero byte before message must fail with ENOTSUP");

    std::vector<std::uint8_t> unterminated = request;
    std::memset(unterminated.data() + 45, 'a', 1024);
    Require(sceKernelSendNotificationRequest(0, unterminated.data(), unterminated.size(), 0) == invalidArgument,
            "unterminated message must fail with EINVAL");

    Require(sceKernelDebugOutText(-1, "text") == invalidArgument,
            "negative debug channel must fail with EINVAL");
    Require(sceKernelDebugOutText(0, nullptr) == badAddress,
            "null debug text must fail with EFAULT");
    errno = 71;
    Require(sceKernelDebugOutText(0, reinterpret_cast<const char*>(0x10)) == badAddress,
            "unreadable debug text must fail with EFAULT");
    Require(errno == 71, "unreadable debug text changed host errno");
}

void TestKernelNotificationHostStderr() {
    std::FILE* const hostCapture = TemporaryFile::Open();
    Require(hostCapture != nullptr, "open host capture file");
#ifdef _WIN32
    const int savedHostStderr = ::_dup(2);
    Require(savedHostStderr >= 0 && ::_dup2(::_fileno(hostCapture), 2) == 0,
            "redirect host stderr to capture file");
#else
    const int savedHostStderr = ::dup(2);
    Require(savedHostStderr >= 0 && ::dup2(::fileno(hostCapture), 2) == 2,
            "redirect host stderr to capture file");
#endif

    int guestPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(guestPipe) == 0, "create guest pipe");
    Require(dup2_nid_postfix(guestPipe[1], 2) == 2, "redirect guest fd 2 onto pipe");

#ifdef _WIN32
    const HANDLE savedStdErrorHandle = GetStdHandle(STD_ERROR_HANDLE);
    Require(SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE) != 0,
            "invalidate STD_ERROR_HANDLE after preserving host stderr");
#endif

    std::vector<std::uint8_t> request(3120, 0);
    const std::int32_t systemTextType = 5;
    std::memcpy(request.data(), &systemTextType, sizeof(systemTextType));
    constexpr const char* message = "host-stderr-check";
    std::memcpy(request.data() + 45, message, std::strlen(message) + 1);

    errno = 71;
    Require(sceKernelSendNotificationRequest(0, request.data(), request.size(), 0) == 0,
            "sceKernelSendNotificationRequest succeeds when guest fd 2 is redirected");
    Require(errno == 71, "sceKernelSendNotificationRequest changed host errno on success");

    const std::string longPayload(0x10000, 'x');
    errno = 71;
    Require(sceKernelDebugOutText(0, longPayload.c_str()) == 0,
            "sceKernelDebugOutText succeeds with 0x10000-byte payload");
    Require(errno == 71, "sceKernelDebugOutText changed host errno on success");

    Require(write_nid_postfix(2, "Z", 1) == 1, "write sentinel byte to redirected guest fd 2");

#ifdef _WIN32
    Require(SetStdHandle(STD_ERROR_HANDLE, savedStdErrorHandle) != 0, "restore STD_ERROR_HANDLE");
    Require(::_dup2(savedHostStderr, 2) == 0 && ::_close(savedHostStderr) == 0, "restore host stderr");
#else
    Require(::dup2(savedHostStderr, 2) == 2 && ::close(savedHostStderr) == 0, "restore host stderr");
#endif

    char guestBuf[64]{};
    const auto guestRead = read_nid_postfix(guestPipe[0], guestBuf, sizeof(guestBuf));
    Require(guestRead == 1 && guestBuf[0] == 'Z',
            "notification or debug output leaked into redirected guest fd 2");
    Require(close_nid_postfix(guestPipe[1]) == 0 && close_nid_postfix(guestPipe[0]) == 0,
            "close guest pipe descriptors");

    std::rewind(hostCapture);
    std::string captured;
    char chunk[4096];
    while (const auto count = std::fread(chunk, 1, sizeof(chunk), hostCapture)) {
        captured.append(chunk, count);
    }
    Require(std::fclose(hostCapture) == 0, "close host capture file");

    const std::string expected = std::string("[notification] host-stderr-check\n[debug:0] ") + longPayload;
    Require(captured == expected, "host stderr capture did not match expected notification and debug text");
}

}

int main() {
    TestSend();
    TestSendById();
    TestBanner();
    TestHide();
    TestKernelNotificationErrorPaths();
    TestKernelNotificationHostStderr();
    return 0;
}
