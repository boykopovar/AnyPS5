#include "SceTypes.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {

int APS5_VABI sceSystemTtsCancel(std::int32_t speechId);
int APS5_VABI sceSystemTtsIsAccessibilityAvailable(const void* param, std::uint8_t* outAvailable);
int APS5_VABI sceSystemTtsRegisterCallback(void* callback, void* userData);
int APS5_VABI sceSystemTtsSpeakRequest(const void* param);
int APS5_VABI sceSystemTtsUnregisterCallback(void* callback);
int APS5_VABI sceSystemTtsConvertText(const void* param, void* outBuf, std::size_t* outSize);
int APS5_VABI sceSystemTtsPauseResume(void);
int APS5_VABI sceSystemTtsRepeat(void);
int APS5_VABI sceSystemTtsWait(std::int32_t speechId);
int APS5_VABI libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(std::uint32_t* outStatus);

}

namespace {

constexpr int INVALID_PARAM = static_cast<int>(0x819A1001u);
constexpr int NOT_FOUND = static_cast<int>(0x819A1008u);
constexpr int KERNEL_ESRCH = static_cast<int>(0x80020003u);
constexpr std::uint32_t EVF_ATTR_SHARED_MULTI = 0x121u;

struct SpeakParamBuffer {
    std::uint32_t reserved00 = 0;
    std::uint32_t usageContext = 0;
    std::uint32_t engineType = 0;
    std::uint32_t language = 0;
    std::int32_t voiceType = 0;
    std::uint32_t reserved14 = 0;
    std::int32_t speed = 0;
    std::uint32_t volume = 100;
    std::int32_t pitch = 0;
    std::uint32_t reserved24 = 0;
    std::uint8_t text[4100] = {};
};

static_assert(offsetof(SpeakParamBuffer, text) == 0x28);

struct AccessibilityParamBuffer {
    std::uint32_t reserved00 = 0;
    std::uint32_t language = 0;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void DummyCallback(std::int32_t, std::int32_t, void*) {}

template <typename Fn>
void ExpectNotImplemented(Fn&& fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        constexpr const char suffix[] = "not implemented";
        constexpr std::size_t suffixLen = sizeof(suffix) - 1;
        Require(message.size() >= suffixLen &&
                message.compare(message.size() - suffixLen, suffixLen, suffix) == 0);
        caught = true;
    }
    Require(caught);
}

void SetEmbeddedText(SpeakParamBuffer& param, std::size_t length) {
    std::memset(param.text, 'A', length);
    param.text[length] = 0;
}

void SetCloudText(SpeakParamBuffer& param, std::size_t length) {
    auto* chars = reinterpret_cast<std::uint16_t*>(param.text);
    for (std::size_t i = 0; i < length; ++i) {
        chars[i] = static_cast<std::uint16_t>('A');
    }
    chars[length] = 0;
}

void TestCallbacks() {
    Require(sceSystemTtsRegisterCallback(nullptr, nullptr) == INVALID_PARAM);

    int tag1 = 1;
    int tag2 = 2;
    void* fn = reinterpret_cast<void*>(&DummyCallback);
    Require(sceSystemTtsRegisterCallback(fn, &tag1) == 0);
    Require(sceSystemTtsRegisterCallback(fn, &tag2) == 0);

    Require(sceSystemTtsUnregisterCallback(fn) == 0);
    Require(sceSystemTtsUnregisterCallback(fn) == 0);
    Require(sceSystemTtsUnregisterCallback(nullptr) == 0);
}

void TestCancelAndWait() {
    Require(sceSystemTtsCancel(0) == NOT_FOUND);
    Require(sceSystemTtsCancel(1) == NOT_FOUND);
    Require(sceSystemTtsWait(0) == NOT_FOUND);
    Require(sceSystemTtsWait(42) == NOT_FOUND);
}

void TestStatusEventFlag() {
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(nullptr) == INVALID_PARAM);

    errno = 73;
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(reinterpret_cast<std::uint32_t*>(0x10)) == INVALID_PARAM);
    Require(errno == 73);

    std::uint32_t statusOut = 99u;
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(&statusOut) == KERNEL_ESRCH);
    Require(statusOut == 0u);

    KernelEventFlag evf = nullptr;
    Require(sceKernelCreateEventFlag(&evf, "SceSystemTtsStatus", EVF_ATTR_SHARED_MULTI, 0x10ULL, nullptr) == 0);

    statusOut = 99u;
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(&statusOut) == 0);
    Require(statusOut == 0u);

    Require(sceKernelSetEventFlag(evf, 2ULL) == 0);
    statusOut = 99u;
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(&statusOut) == 0);
    Require(statusOut == 1u);

    Require(sceKernelSetEventFlag(evf, 1ULL) == 0);
    statusOut = 99u;
    Require(libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(&statusOut) == 0);
    Require(statusOut == 1u);

    Require(sceKernelDeleteEventFlag(evf) == 0);
}

void TestSpeakAndConvertValidation() {
    std::uint8_t outBuf[16] = {};
    std::size_t outSize = sizeof(outBuf);

    Require(sceSystemTtsSpeakRequest(nullptr) == INVALID_PARAM);
    Require(sceSystemTtsConvertText(nullptr, outBuf, &outSize) == INVALID_PARAM);

    errno = 74;
    Require(sceSystemTtsSpeakRequest(reinterpret_cast<const void*>(0x10)) == INVALID_PARAM);
    Require(errno == 74);
    errno = 75;
    Require(sceSystemTtsConvertText(reinterpret_cast<const void*>(0x10), outBuf, &outSize) == INVALID_PARAM);
    Require(errno == 75);

    SpeakParamBuffer validEmbedded{};
    SetEmbeddedText(validEmbedded, 2);
    Require(sceSystemTtsConvertText(&validEmbedded, nullptr, &outSize) == INVALID_PARAM);
    Require(sceSystemTtsConvertText(&validEmbedded, outBuf, nullptr) == INVALID_PARAM);

    ExpectNotImplemented([&]() { sceSystemTtsSpeakRequest(&validEmbedded); });
    ExpectNotImplemented([&]() { sceSystemTtsConvertText(&validEmbedded, outBuf, &outSize); });

    SpeakParamBuffer validCloud{};
    validCloud.engineType = 1u;
    validCloud.language = 6u;
    validCloud.voiceType = 18;
    validCloud.speed = 10;
    validCloud.volume = 200u;
    validCloud.pitch = -10;
    SetCloudText(validCloud, 2);
    ExpectNotImplemented([&]() { sceSystemTtsSpeakRequest(&validCloud); });
    ExpectNotImplemented([&]() { sceSystemTtsConvertText(&validCloud, outBuf, &outSize); });

    auto checkInvalid = [&](const SpeakParamBuffer& bad) {
        Require(sceSystemTtsSpeakRequest(&bad) == INVALID_PARAM);
        Require(sceSystemTtsConvertText(&bad, outBuf, &outSize) == INVALID_PARAM);
    };

    {
        SpeakParamBuffer p = validEmbedded;
        p.usageContext = 5u;
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validEmbedded;
        p.engineType = 2u;
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validEmbedded;
        p.language = 6u;
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validCloud;
        p.language = 12u;
        checkInvalid(p);
    }
    for (const std::int32_t badVoice : {9, -2, 19}) {
        SpeakParamBuffer p = validEmbedded;
        p.voiceType = badVoice;
        checkInvalid(p);
    }
    for (const std::int32_t badSpeed : {3, -3}) {
        SpeakParamBuffer p = validEmbedded;
        p.speed = badSpeed;
        checkInvalid(p);
    }
    for (const std::int32_t badSpeed : {11, -11}) {
        SpeakParamBuffer p = validCloud;
        p.speed = badSpeed;
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validEmbedded;
        p.volume = 201u;
        checkInvalid(p);
    }
    for (const std::int32_t badPitch : {11, -11}) {
        SpeakParamBuffer p = validEmbedded;
        p.pitch = badPitch;
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validEmbedded;
        SetEmbeddedText(p, 0);
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validCloud;
        SetCloudText(p, 0);
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validEmbedded;
        SetEmbeddedText(p, 1025);
        checkInvalid(p);
    }
    {
        SpeakParamBuffer p = validCloud;
        SetCloudText(p, 2049);
        checkInvalid(p);
    }
}

void TestAccessibilityAndArbitrator() {
    std::uint8_t available = 0xA5u;
    AccessibilityParamBuffer validParam{0u, 0u};

    Require(sceSystemTtsIsAccessibilityAvailable(nullptr, &available) == INVALID_PARAM);
    Require(sceSystemTtsIsAccessibilityAvailable(&validParam, nullptr) == INVALID_PARAM);

    errno = 76;
    Require(sceSystemTtsIsAccessibilityAvailable(&validParam, reinterpret_cast<std::uint8_t*>(0x10)) == INVALID_PARAM);
    Require(errno == 76);

    errno = 77;
    available = 0xA5u;
    Require(sceSystemTtsIsAccessibilityAvailable(reinterpret_cast<const void*>(0x10), &available) == INVALID_PARAM);
    Require(errno == 77);
    Require(available == 0u);

    AccessibilityParamBuffer badLang{0u, 12u};
    available = 0xA5u;
    Require(sceSystemTtsIsAccessibilityAvailable(&badLang, &available) == INVALID_PARAM);
    Require(available == 0u);

    available = 0xA5u;
    ExpectNotImplemented([&]() { sceSystemTtsIsAccessibilityAvailable(&validParam, &available); });
    Require(available == 0u);

    ExpectNotImplemented([]() { sceSystemTtsPauseResume(); });
    ExpectNotImplemented([]() { sceSystemTtsRepeat(); });
}

}

int main() {
    TestCallbacks();
    TestCancelAndWait();
    TestStatusEventFlag();
    TestSpeakAndConvertValidation();
    TestAccessibilityAndArbitrator();
    return 0;
}
