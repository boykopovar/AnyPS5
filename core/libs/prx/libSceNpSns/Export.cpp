#include "prx/libSceNpSns/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int kErrNotInitialized = static_cast<int>(0x80550002);
constexpr int kErrInvalidArg = static_cast<int>(0x80550003);
constexpr int kErrOutOfMemory = static_cast<int>(0x80550005);
constexpr int kErrInvalidSize = static_cast<int>(0x80550011);
constexpr int kErrAborted = static_cast<int>(0x80550012);
constexpr int kErrRequestMax = static_cast<int>(0x80550013);
constexpr int kErrIdNotFound = static_cast<int>(0x80550014);

constexpr int kErrFbUnknown = static_cast<int>(0x80552601);
constexpr int kErrFbInvalidArg = static_cast<int>(0x80552602);
constexpr int kErrFbAborted = static_cast<int>(0x80552606);
constexpr int kErrFbNoConsent = static_cast<int>(0x80552607);

constexpr int kErrYtUnknown = static_cast<int>(0x80552d01);
constexpr int kErrYtInvalidArg = static_cast<int>(0x80552d02);
constexpr int kErrYtRequestMax = static_cast<int>(0x80552d04);
constexpr int kErrYtAborted = static_cast<int>(0x80552d06);
constexpr int kErrYtNoConsent = static_cast<int>(0x80552d07);
constexpr int kErrYtRefreshToken = static_cast<int>(0x80552d0a);
constexpr int kErrYtUnlinked = static_cast<int>(0x80552d0b);

constexpr int kErrIpmiBase = static_cast<int>(0x80559400);
constexpr int kErrIpmiAlt = static_cast<int>(0x80559401);
constexpr int kErrIpmiNoConsent = static_cast<int>(0x80559402);
constexpr int kErrIpmiUnlinked = static_cast<int>(0x80559403);

constexpr int kReqIdBase = 0x60000000;
constexpr int kReqIdMax = 0x6fffffff;
constexpr std::size_t kMaxIntRequests = 32;
constexpr int kMaxServiceRequests = 16;

constexpr std::uint64_t kTwitchParamSize = 0x30;
constexpr std::size_t kTwitchReservedOffset = 0x0c;
constexpr std::size_t kTwitchReservedSize = 0x20;

constexpr std::uint64_t kYouTubeParamSize = 0xc30;
constexpr std::size_t kYouTubeRequiredStrOffset = 0x0c;
constexpr std::size_t kYouTubeReservedOffset = 0x0c0c;
constexpr std::size_t kYouTubeReservedSize = 0x20;

enum class RequestOwner {
    Internal,
    Twitch,
    YouTube,
};

struct RequestEntry {
    int id = 0;
    bool aborted = false;
    RequestOwner owner = RequestOwner::Internal;
};

std::atomic<bool> g_initialized{true};
std::mutex g_requestMutex;
std::vector<RequestEntry> g_requests;
int g_youtubeRequests = 0;
int g_twitchRequests = 0;

int CreateRequestForOwner(RequestOwner owner) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    std::lock_guard<std::mutex> lock(g_requestMutex);
    if (owner == RequestOwner::Twitch && g_twitchRequests >= kMaxServiceRequests) return kErrRequestMax;
    if (owner == RequestOwner::YouTube && g_youtubeRequests >= kMaxServiceRequests) return kErrYtRequestMax;
    if (g_requests.size() >= kMaxIntRequests) return kErrRequestMax;
    int candidate = kReqIdBase + 1;
    for (const auto& entry : g_requests) {
        if (entry.id == candidate) {
            if (candidate >= kReqIdMax) return kErrRequestMax;
            ++candidate;
        } else if (entry.id > candidate) {
            break;
        }
    }
    if (candidate > kReqIdMax) return kErrRequestMax;
    RequestEntry entry;
    entry.id = candidate;
    entry.owner = owner;
    auto it = std::lower_bound(g_requests.begin(), g_requests.end(), candidate,
        [](const RequestEntry& lhs, int rhs) { return lhs.id < rhs; });
    g_requests.insert(it, entry);
    if (owner == RequestOwner::Twitch) {
        ++g_twitchRequests;
    } else if (owner == RequestOwner::YouTube) {
        ++g_youtubeRequests;
    }
    return candidate;
}

int DeleteRequestForOwner(int reqId, RequestOwner expectedOwner, bool matchAnyOwner) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (reqId == 0) return kErrIdNotFound;
    std::lock_guard<std::mutex> lock(g_requestMutex);
    for (auto it = g_requests.begin(); it != g_requests.end(); ++it) {
        if (it->id == reqId) {
            if (!matchAnyOwner && it->owner != expectedOwner) return kErrIdNotFound;
            if (it->owner == RequestOwner::Twitch && g_twitchRequests > 0) {
                --g_twitchRequests;
            } else if (it->owner == RequestOwner::YouTube && g_youtubeRequests > 0) {
                --g_youtubeRequests;
            }
            g_requests.erase(it);
            return 0;
        }
        if (it->id > reqId) break;
    }
    return kErrIdNotFound;
}

bool IsZeroRange(const std::uint8_t* data, std::size_t offset, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        if (data[offset + i] != 0) return false;
    }
    return true;
}

int MapFacebookError(int err) {
    if (err >= 0) return err;
    if (err == kErrIpmiBase || err == kErrIpmiAlt || err == kErrIpmiUnlinked) return kErrFbUnknown;
    if (err == kErrIpmiNoConsent) return kErrFbNoConsent;
    if (err == kErrAborted) return kErrFbAborted;
    if (err == kErrIdNotFound) return kErrFbInvalidArg;
    return err;
}

int MapYouTubeError(int err) {
    if (err >= 0) return err;
    if (err == kErrAborted || err == kErrFbAborted) return kErrYtAborted;
    if (err == kErrIdNotFound || err == kErrInvalidArg || err == kErrInvalidSize) return kErrYtInvalidArg;
    if (err == kErrFbNoConsent || err == kErrIpmiNoConsent) return kErrYtNoConsent;
    if (err == kErrIpmiBase) return kErrYtUnknown;
    if (err == kErrIpmiAlt) return kErrYtRefreshToken;
    if (err == kErrIpmiUnlinked) return kErrYtUnlinked;
    return err;
}

int AbortRequestForOwner(int reqId, RequestOwner expectedOwner, bool matchAnyOwner) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (reqId == 0) return kErrInvalidArg;
    std::lock_guard<std::mutex> lock(g_requestMutex);
    for (auto& entry : g_requests) {
        if (entry.id == reqId) {
            if (!matchAnyOwner && entry.owner != expectedOwner) return kErrIdNotFound;
            entry.aborted = true;
            return 0;
        }
        if (entry.id > reqId) break;
    }
    return kErrIdNotFound;
}

int ValidateRequestForUse(int reqId, RequestOwner expectedOwner = RequestOwner::Internal, bool matchAnyOwner = true) {
    if (reqId == 0) return kErrIdNotFound;
    std::lock_guard<std::mutex> lock(g_requestMutex);
    for (const auto& entry : g_requests) {
        if (entry.id == reqId) {
            if (!matchAnyOwner && entry.owner != expectedOwner) return kErrIdNotFound;
            if (entry.aborted) return kErrAborted;
            return 0;
        }
    }
    return kErrIdNotFound;
}

bool TryWriteZeroFlag(int* outFlag) {
    if (outFlag == nullptr) return true;
    const std::int32_t zero = 0;
    return GuestCopyOut_nid_no_patch(outFlag, &zero, sizeof(zero));
}

}

extern "C" {

int APS5_VABI sceNpSnsIntCreateRequest(void) {
    return CreateRequestForOwner(RequestOwner::Internal);
}

int APS5_VABI sceNpSnsIntDeleteRequest(int reqId) {
    return DeleteRequestForOwner(reqId, RequestOwner::Internal, true);
}

int APS5_VABI sceNpSnsIntAbortRequest(int reqId) {
    return AbortRequestForOwner(reqId, RequestOwner::Internal, true);
}

int APS5_VABI sceNpSnsIntFbGetTitleInfo(const void* arg1, const void* arg2, void* outInfo) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (arg1 == nullptr || arg2 == nullptr || outInfo == nullptr) return kErrInvalidArg;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntLinkedStatus(int reqId, int userId, int serviceType, void* outStatus) {
    static_cast<void>(userId);
    static_cast<void>(serviceType);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (outStatus == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntFbGetSystemAccessToken(int reqId, int userId, void* outToken) {
    static_cast<void>(userId);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (outToken == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntTwGetSystemAccessToken(int reqId, int userId, void* outToken) {
    static_cast<void>(userId);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (outToken == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntYtGetAccessToken(int reqId, int userId, const char* arg3, const char* arg4, int* outFlag, void* outBuf, void* outToken) {
    static_cast<void>(userId);
    static_cast<void>(arg4);
    static_cast<void>(outBuf);
    if (!TryWriteZeroFlag(outFlag)) return kErrInvalidArg;
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (arg3 == nullptr || outToken == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntYtRefreshMasterToken(int reqId, int userId, const void* arg3) {
    static_cast<void>(userId);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (arg3 == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntUnlink(int reqId, int userId, int serviceType) {
    static_cast<void>(userId);
    static_cast<void>(serviceType);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntFbGetGameAccessToken(int reqId, int userId, const void* arg3, const void* arg4, const void* arg5, int* outFlag, void* outBuf, void* outToken) {
    static_cast<void>(userId);
    static_cast<void>(arg3);
    if (!TryWriteZeroFlag(outFlag)) return kErrInvalidArg;
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (outFlag == nullptr || outToken == nullptr || arg5 == nullptr || arg4 == nullptr || outBuf == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsIntFbGetGameAccessTokenAllowed(int reqId, int userId, const void* arg3, const void* arg4, const void* arg5, void* outAllowed) {
    static_cast<void>(userId);
    static_cast<void>(arg3);
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (arg4 == nullptr || arg5 == nullptr || outAllowed == nullptr) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsFacebookAbortRequest(int reqId) {
    return MapFacebookError(AbortRequestForOwner(reqId, RequestOwner::Internal, true));
}

int APS5_VABI sceNpSnsTwitchCreateRequest(void) {
    return CreateRequestForOwner(RequestOwner::Twitch);
}

int APS5_VABI sceNpSnsTwitchDeleteRequest(int reqId) {
    return DeleteRequestForOwner(reqId, RequestOwner::Twitch, false);
}

int APS5_VABI sceNpSnsTwitchAbortRequest(int reqId) {
    const int ret = AbortRequestForOwner(reqId, RequestOwner::Twitch, false);
    return ret < 0 ? ret : 0;
}

int APS5_VABI sceNpSnsTwitchGetAccessToken(int reqId, const void* param, void* outToken) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (param == nullptr || outToken == nullptr) return kErrInvalidArg;
    std::array<std::uint8_t, kTwitchParamSize> localParam{};
    std::uint64_t paramSize = 0;
    if (!GuestCopyIn_nid_no_patch(&paramSize, param, sizeof(paramSize))) return kErrInvalidArg;
    if (paramSize != kTwitchParamSize) return kErrInvalidSize;
    if (!GuestCopyIn_nid_no_patch(localParam.data(), param, localParam.size())) return kErrInvalidArg;
    if (!IsZeroRange(localParam.data(), kTwitchReservedOffset, kTwitchReservedSize)) return kErrInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId, RequestOwner::Twitch, false);
    if (reqStatus < 0) return reqStatus;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsYouTubeCreateRequest(void) {
    const int reqId = CreateRequestForOwner(RequestOwner::YouTube);
    return reqId >= 0 ? reqId : MapYouTubeError(reqId);
}

int APS5_VABI sceNpSnsYouTubeDeleteRequest(int reqId) {
    const int ret = DeleteRequestForOwner(reqId, RequestOwner::YouTube, false);
    return ret >= 0 ? ret : MapYouTubeError(ret);
}

int APS5_VABI sceNpSnsYouTubeAbortRequest(int reqId) {
    const int ret = AbortRequestForOwner(reqId, RequestOwner::YouTube, false);
    return MapYouTubeError(ret);
}

int APS5_VABI sceNpSnsYouTubeGetAccessToken(int reqId, const void* param, void* outToken) {
    if (!g_initialized.load(std::memory_order_acquire)) return kErrNotInitialized;
    if (param == nullptr || outToken == nullptr) return kErrYtInvalidArg;
    std::uint64_t paramSize = 0;
    if (!GuestCopyIn_nid_no_patch(&paramSize, param, sizeof(paramSize))) return kErrYtInvalidArg;
    if (paramSize != kYouTubeParamSize) return MapYouTubeError(kErrInvalidSize);
    std::vector<std::uint8_t> localParam(kYouTubeParamSize);
    if (!GuestCopyIn_nid_no_patch(localParam.data(), param, localParam.size())) return kErrYtInvalidArg;
    if (!IsZeroRange(localParam.data(), kYouTubeReservedOffset, kYouTubeReservedSize)) return kErrYtInvalidArg;
    if (localParam[kYouTubeRequiredStrOffset] == 0) return kErrYtInvalidArg;
    const int reqStatus = ValidateRequestForUse(reqId, RequestOwner::YouTube, false);
    if (reqStatus < 0) return MapYouTubeError(reqStatus);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
