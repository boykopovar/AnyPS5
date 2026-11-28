#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <cerrno>
#include <string>

namespace {

constexpr int invalidArgument = static_cast<int>(0x80A10003u);
constexpr int shellInvalidArgument = static_cast<int>(0x80E30005u);
constexpr std::uintptr_t untouchedHandle = 0x5A5A5A5A5A5A5A5Au;
constexpr std::uint32_t untouchedStatus = 0x5A5A5A5Au;
constexpr std::int32_t untouchedResult = 0x5A5A5A5A;
constexpr std::uint8_t untouchedFlag = 0x5A;

void Require(bool value) { if (!value) std::abort(); }

struct Codec {
    int (APS5_VABI* init)(std::uintptr_t*);
    int (APS5_VABI* isActivated)(std::uint8_t*);
    int (APS5_VABI* start)(std::uintptr_t);
    int (APS5_VABI* getStatus)(std::uintptr_t, std::uint32_t*, std::int32_t*);
    int (APS5_VABI* abort)(std::uintptr_t);
    int (APS5_VABI* term)(std::uintptr_t);
};

void TestShellCalls(std::intptr_t expectedHandle) {
    Require(sceShellCoreUtilActivateInit(7, nullptr) == shellInvalidArgument);
    errno = 71;
    Require(sceShellCoreUtilActivateInit(7, reinterpret_cast<std::intptr_t*>(0x10)) == shellInvalidArgument);
    Require(errno == 71);
    std::intptr_t handle = 0;
    Require(sceShellCoreUtilActivateInit(7, &handle) == 0 && handle == expectedHandle);
    std::int32_t status = static_cast<std::int32_t>(untouchedStatus);
    std::int32_t result = untouchedResult;
    Require(sceShellCoreUtilActivateGetStatus(0, nullptr, &result) == shellInvalidArgument && result == untouchedResult);
    Require(sceShellCoreUtilActivateGetStatus(0, &status, nullptr) == shellInvalidArgument && status == static_cast<std::int32_t>(untouchedStatus));
    Require(sceShellCoreUtilActivateGetStatus(0, &status, &result) == 0 && status == 2 && result == 0);
    errno = 71;
    Require(sceShellCoreUtilActivateGetStatus(0, reinterpret_cast<std::int32_t*>(0x10), &result) == shellInvalidArgument);
    Require(sceShellCoreUtilActivateGetStatus(0, &status, reinterpret_cast<std::int32_t*>(0x10)) == shellInvalidArgument);
    Require(errno == 71);
    Require(sceShellCoreUtilActivateIsActivated(7, nullptr) == shellInvalidArgument);
    errno = 71;
    Require(sceShellCoreUtilActivateIsActivated(7, reinterpret_cast<bool*>(0x10)) == shellInvalidArgument);
    Require(errno == 71);
    bool activated = false;
    Require(sceShellCoreUtilActivateIsActivated(-1, &activated) == 0 && activated);
    for (std::intptr_t value : {std::intptr_t{0}, handle, std::intptr_t{-1}}) {
        Require(sceShellCoreUtilActivateStart(value) == 0);
        Require(sceShellCoreUtilActivateStartAsync(value) == 0);
        Require(sceShellCoreUtilActivateAbort(value) == 0);
        Require(sceShellCoreUtilActivateTerm(value) == 0);
    }
    Require(sceShellCoreUtilActivateRecordActivation() == 0);
}

void TestCodec(const Codec& codec, std::uintptr_t expectedHandle) {
    Require(codec.init(nullptr) == invalidArgument);
    std::uintptr_t handle = untouchedHandle;
    Require(codec.init(&handle) == 0 && handle == expectedHandle);
    Require(codec.isActivated(nullptr) == invalidArgument);
    std::uint8_t activated = untouchedFlag;
    Require(codec.isActivated(&activated) == 0 && activated == 1);
    std::uint32_t status = untouchedStatus;
    std::int32_t result = untouchedResult;
    Require(codec.getStatus(0, &status, &result) == invalidArgument);
    Require(codec.getStatus(handle, nullptr, &result) == invalidArgument);
    Require(codec.getStatus(handle, &status, nullptr) == invalidArgument);
    Require(status == untouchedStatus && result == untouchedResult);
    for (auto call : {codec.start, codec.abort, codec.term}) {
        Require(call(0) == invalidArgument);
        Require(call(handle) == 0);
        Require(call(untouchedHandle) == 0);
    }
    Require(codec.getStatus(handle, &status, &result) == 0 && status == 2 && result == 0);
    Require(codec.getStatus(untouchedHandle, &status, &result) == 0 && status == 2 && result == 0);
}

void TestHostConfirmationText() {
    const std::string prefix = "A game asks to open this address in your default web browser:\n\n";
    const std::string suffix = "\n\nOpen it only if you trust it.";

    const std::string malicious = std::string("https://example.com/a\nb\rc") + '\x1b' + "d" + '\x7f' + "e";
    const std::string text = HostConfirmationText(malicious);
    Require(text.rfind(prefix, 0) == 0);
    Require(text.size() >= prefix.size() + suffix.size());
    Require(text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
    const std::string body = text.substr(prefix.size(), text.size() - prefix.size() - suffix.size());
    Require(body.find("%0A") != std::string::npos);
    Require(body.find("%0D") != std::string::npos);
    Require(body.find("%1B") != std::string::npos);
    Require(body.find("%7F") != std::string::npos);
    for (const char ch : body) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        Require(byte >= 0x20 && byte != 0x7F);
    }

    const std::string normal = "https://example.com/path?q=1&r=2#frag";
    Require(HostConfirmationText(normal) == prefix + normal + suffix);

    const std::string longUrl = "https://example.com/" + std::string(100, 'a');
    Require(HostConfirmationText(longUrl) == prefix + longUrl.substr(0, 96) + '\n' + longUrl.substr(96) + suffix);
}

}

int main() {
    TestCodec({sceSystemServiceActivateHevcInit, sceSystemServiceActivateHevcIsActivated, sceSystemServiceActivateHevcStart,
               sceSystemServiceActivateHevcGetStatus, sceSystemServiceActivateHevcAbort, sceSystemServiceActivateHevcTerm}, 0x1000);
    TestCodec({sceSystemServiceActivateHevcSoftInit, sceSystemServiceActivateHevcSoftIsActivated, sceSystemServiceActivateHevcSoftStart,
               sceSystemServiceActivateHevcSoftGetStatus, sceSystemServiceActivateHevcSoftAbort, sceSystemServiceActivateHevcSoftTerm}, 0x1004);
    TestCodec({sceSystemServiceActivateMpeg2Init, sceSystemServiceActivateMpeg2IsActivated, sceSystemServiceActivateMpeg2Start,
               sceSystemServiceActivateMpeg2GetStatus, sceSystemServiceActivateMpeg2Abort, sceSystemServiceActivateMpeg2Term}, 0x1008);
    TestShellCalls(0x100C);
    Require(sceSystemServiceActivateHevc() == 0);
    Require(sceSystemServiceActivateHevcSoft() == 0);
    TestHostConfirmationText();
    return 0;
}
