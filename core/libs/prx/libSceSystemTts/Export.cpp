#include "prx/libc/include/general/ExportMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

constexpr int SCE_SYSTEM_TTS_ERROR_INVALID_PARAM = static_cast<int>(0x819A1001u);
constexpr int SCE_SYSTEM_TTS_ERROR_NOT_FOUND = static_cast<int>(0x819A1008u);
constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010u);
constexpr std::uint32_t SCE_KERNEL_EVF_WAITMODE_OR = 2u;
constexpr std::uint32_t CLOUD_LANG_MASK = 0x7F2FFFu;
constexpr std::uint32_t EMBEDDED_LANG_MASK = 0x54003Fu;
constexpr std::size_t MAX_EMBEDDED_TEXT_LEN = 1024u;
constexpr std::size_t MAX_CLOUD_TEXT_LEN = 2048u;

struct SpeakParamHeader {
    std::uint32_t reserved00;
    std::uint32_t usageContext;
    std::uint32_t engineType;
    std::uint32_t language;
    std::int32_t voiceType;
    std::uint32_t reserved14;
    std::int32_t speed;
    std::uint32_t volume;
    std::int32_t pitch;
    std::uint32_t reserved24;
};

static_assert(sizeof(SpeakParamHeader) == 0x28);
static_assert(offsetof(SpeakParamHeader, usageContext) == 0x04);
static_assert(offsetof(SpeakParamHeader, engineType) == 0x08);
static_assert(offsetof(SpeakParamHeader, language) == 0x0c);
static_assert(offsetof(SpeakParamHeader, voiceType) == 0x10);
static_assert(offsetof(SpeakParamHeader, speed) == 0x18);
static_assert(offsetof(SpeakParamHeader, volume) == 0x1c);
static_assert(offsetof(SpeakParamHeader, pitch) == 0x20);

std::mutex g_callbackMutex;
std::map<std::uintptr_t, void*> g_callbacks;

std::mutex g_itemMutex;
std::map<std::int32_t, std::uintptr_t> g_items;

std::mutex g_statusEvfMutex;
KernelEventFlag g_statusEvf = nullptr;

bool CopyInPreservingErrno(void* dst, const void* src, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, bytes);
    errno = savedErrno;
    return ok;
}

bool CopyOutPreservingErrno(void* dst, const void* src, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, bytes);
    errno = savedErrno;
    return ok;
}

bool IsLanguageSupported(std::uint32_t language, std::uint32_t mask) {
    return language <= 22u && ((mask >> language) & 1u) != 0u;
}

bool ValidateSpeakParam(const void* guestParam) {
    if (!guestParam) {
        return false;
    }
    SpeakParamHeader header{};
    if (!CopyInPreservingErrno(&header, guestParam, sizeof(header))) {
        return false;
    }
    if (header.usageContext > 4u || header.engineType > 1u) {
        return false;
    }
    const std::uint32_t langMask = header.engineType == 1u ? CLOUD_LANG_MASK : EMBEDDED_LANG_MASK;
    if (!IsLanguageSupported(header.language, langMask)) {
        return false;
    }
    const std::uint32_t voiceKey = static_cast<std::uint32_t>(header.voiceType) + 1u;
    if (voiceKey > 0x13u || voiceKey == 0xAu) {
        return false;
    }
    if (header.engineType == 1u) {
        if (header.speed < -10 || header.speed > 10) {
            return false;
        }
    } else {
        if (header.speed < -2 || header.speed > 2) {
            return false;
        }
    }
    if (header.volume > 200u) {
        return false;
    }
    if (header.pitch < -10 || header.pitch > 10) {
        return false;
    }

    const auto* textBase = static_cast<const std::uint8_t*>(guestParam) + sizeof(SpeakParamHeader);
    if (header.engineType == 1u) {
        for (std::size_t index = 0; index <= MAX_CLOUD_TEXT_LEN; ++index) {
            std::uint16_t ch = 0;
            if (!CopyInPreservingErrno(&ch, textBase + index * sizeof(ch), sizeof(ch))) {
                return false;
            }
            if (ch == 0u) {
                return index >= 1u;
            }
        }
        return false;
    }
    for (std::size_t index = 0; index <= MAX_EMBEDDED_TEXT_LEN; ++index) {
        std::uint8_t ch = 0;
        if (!CopyInPreservingErrno(&ch, textBase + index, sizeof(ch))) {
            return false;
        }
        if (ch == 0u) {
            return index >= 1u;
        }
    }
    return false;
}

}

extern "C" {

int APS5_VABI sceSystemTtsCancel(std::int32_t speechId) {
    {
        std::lock_guard lock(g_itemMutex);
        if (g_items.find(speechId) == g_items.end()) {
            return SCE_SYSTEM_TTS_ERROR_NOT_FOUND;
        }
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsIsAccessibilityAvailable(const void* param, std::uint8_t* outAvailable) {
    if (!param || !outAvailable) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    const std::uint8_t zero = 0;
    if (!CopyOutPreservingErrno(outAvailable, &zero, sizeof(zero))) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    std::uint32_t language = 0;
    if (!CopyInPreservingErrno(&language, static_cast<const std::uint8_t*>(param) + 4, sizeof(language))) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    if (!IsLanguageSupported(language, CLOUD_LANG_MASK)) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsRegisterCallback(void* callback, void* userData) {
    if (!callback) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    std::lock_guard lock(g_callbackMutex);
    g_callbacks.emplace(reinterpret_cast<std::uintptr_t>(callback), userData);
    return 0;
}

int APS5_VABI sceSystemTtsSpeakRequest(const void* param) {
    if (!ValidateSpeakParam(param)) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsUnregisterCallback(void* callback) {
    std::lock_guard lock(g_callbackMutex);
    g_callbacks.erase(reinterpret_cast<std::uintptr_t>(callback));
    return 0;
}

int APS5_VABI sceSystemTtsConvertText(const void* param, void* outBuf, std::size_t* outSize) {
    if (!outBuf || !outSize || !ValidateSpeakParam(param)) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsPauseResume(void) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsRepeat(void) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsWait(std::int32_t speechId) {
    {
        std::lock_guard lock(g_itemMutex);
        if (g_items.find(speechId) == g_items.end()) {
            return SCE_SYSTEM_TTS_ERROR_NOT_FOUND;
        }
    }
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch(std::uint32_t* outStatus) {
    if (!outStatus) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    const std::uint32_t zero = 0;
    if (!CopyOutPreservingErrno(outStatus, &zero, sizeof(zero))) {
        return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
    }
    KernelEventFlag evf = nullptr;
    {
        std::lock_guard lock(g_statusEvfMutex);
        if (!g_statusEvf) {
            const int openRet = sceKernelOpenEventFlag(&g_statusEvf, "SceSystemTtsStatus");
            if (openRet < 0) {
                g_statusEvf = nullptr;
                return openRet;
            }
        }
        evf = g_statusEvf;
    }
    std::uint64_t pattern = 0;
    KernelUseconds timeout = 100000u;
    const int waitRet = sceKernelWaitEventFlag(evf, UINT64_MAX, SCE_KERNEL_EVF_WAITMODE_OR, &pattern, &timeout);
    if (waitRet < 0 && waitRet != SCE_KERNEL_ERROR_EBUSY) {
        return waitRet;
    }
    if ((pattern & 0xfULL) != 0ULL) {
        const std::uint32_t one = 1;
        if (!CopyOutPreservingErrno(outStatus, &one, sizeof(one))) {
            return SCE_SYSTEM_TTS_ERROR_INVALID_PARAM;
        }
    }
    return 0;
}
APS5_EXPORT("aIz2UUh6oi0", libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch);

}
