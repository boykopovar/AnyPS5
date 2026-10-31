#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int NP_ERROR_INVALID_PLATFORM_TYPE = static_cast<int>(0x80550004);
constexpr int NP_UTIL_ERROR_NOT_MATCH = static_cast<int>(0x80550609);
constexpr int NP_UTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550601);
constexpr int NP_UTIL_ERROR_INVALID_NP_ID = static_cast<int>(0x80550605);
constexpr std::uint32_t PLATFORM_TAGS[] = {0, 0x00337370, 0x32707370, 0x00347370};
constexpr std::uint32_t PLATFORM_TAG_COMPANION = 0x706D6F63;
constexpr std::uint32_t PLATFORM_TYPE_COMPANION = 4;
constexpr int NP_LW_COND_ERROR_TIMEDOUT = static_cast<int>(0x8055800B);
constexpr int NP_LW_MUTEX_ERROR_BUSY = static_cast<int>(0x8055800F);
constexpr int KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
constexpr std::size_t ONLINE_ID_MAX_LENGTH = 16;
constexpr int MUTEX_TYPE_RECURSIVE = 2;
constexpr KernelClockid CLOCK_MONOTONIC_ID = 4;

static_assert(sizeof(NpIdFull) == 36);

static_assert(sizeof(SceKernelSystemSwVersion) == 0x28);

std::uint32_t platformTag(const NpIdFull& npId) {
    std::uint32_t tag = 0;
    std::memcpy(&tag, npId.opt + 4, sizeof(tag));
    return tag;
}

void setPlatformTag(NpIdFull& npId, std::uint32_t tag) {
    std::memcpy(npId.opt + 4, &tag, sizeof(tag));
}

int platformTypeOf(const NpIdFull& npId) {
    if (npId.opt[4] == 0) return 0;
    const std::uint32_t tag = platformTag(npId);
    for (std::size_t type = 1; type < std::size(PLATFORM_TAGS); ++type) {
        if (PLATFORM_TAGS[type] == tag) return static_cast<int>(type);
    }
    return NP_ERROR_INVALID_PLATFORM_TYPE;
}

int canonicalizeNpId(NpIdFull* npId, std::int32_t replace, std::uint32_t tag) {
    if (!npId) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (replace != 0) setPlatformTag(*npId, 0);
    if (npId->reserved[0] != 1) return NP_UTIL_ERROR_INVALID_NP_ID;
    if (npId->opt[4] == 0) setPlatformTag(*npId, tag);
    return 0;
}

int compareBytes(const std::uint8_t* lhs, const std::uint8_t* rhs, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (lhs[i] != rhs[i]) return lhs[i] < rhs[i] ? -1 : 1;
    }
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpCmpOnlineId(const NpOnlineId* onlineId1, const NpOnlineId* onlineId2) {
    if (!onlineId1 || !onlineId2) return NP_ERROR_INVALID_ARGUMENT;
    if (std::strncmp(onlineId1->data, onlineId2->data, ONLINE_ID_MAX_LENGTH) != 0) return NP_UTIL_ERROR_NOT_MATCH;
    return 0;
}

int APS5_VABI sceNpCmpNpIdInOrder(const NpIdFull* npId1, const NpIdFull* npId2, std::int32_t* order) {
    if (!npId1 || !npId2 || !order) return NP_ERROR_INVALID_ARGUMENT;
    int compare = std::strncmp(npId1->handle.data, npId2->handle.data, ONLINE_ID_MAX_LENGTH);
    if (compare == 0) compare = compareBytes(npId1->opt, npId2->opt, sizeof(npId1->opt));
    if (compare == 0) compare = compareBytes(npId1->reserved, npId2->reserved, sizeof(npId1->reserved));
    *order = compare < 0 ? -1 : compare > 0 ? 1 : 0;
    return 0;
}

int APS5_VABI sceNpGetPlatformType(const NpIdFull* npId) {
    if (!npId) return NP_ERROR_INVALID_ARGUMENT;
    return platformTypeOf(*npId);
}

int APS5_VABI sceNpIntGetPlatformType(const NpIdFull* npId) {
    if (!npId) return NP_ERROR_INVALID_ARGUMENT;
    if (platformTag(*npId) == PLATFORM_TAG_COMPANION) return static_cast<int>(PLATFORM_TYPE_COMPANION);
    return platformTypeOf(*npId);
}

int APS5_VABI sceNpSetPlatformType(NpIdFull* npId, std::uint32_t platformType) {
    if (!npId) return NP_ERROR_INVALID_ARGUMENT;
    if (platformType >= std::size(PLATFORM_TAGS)) return NP_ERROR_INVALID_PLATFORM_TYPE;
    setPlatformTag(*npId, PLATFORM_TAGS[platformType]);
    return 0;
}

int APS5_VABI sceNpIntSetPlatformType(NpIdFull* npId, std::uint32_t platformType) {
    if (!npId) return NP_ERROR_INVALID_ARGUMENT;
    if (platformType == PLATFORM_TYPE_COMPANION) {
        setPlatformTag(*npId, PLATFORM_TAG_COMPANION);
        return 0;
    }
    return sceNpSetPlatformType(npId, platformType);
}

int APS5_VABI sceNpUtilCanonicalizeNpIdForPs4(NpIdFull* npId, std::int32_t replace) {
    return canonicalizeNpId(npId, replace, PLATFORM_TAGS[3]);
}

int APS5_VABI sceNpUtilCanonicalizeNpIdForPsp2(NpIdFull* npId, std::int32_t replace) {
    return canonicalizeNpId(npId, replace, PLATFORM_TAGS[2]);
}

int APS5_VABI sceNpIntIsValidOnlineId(const NpOnlineId* onlineId) {
    if (!onlineId || onlineId->data[ONLINE_ID_MAX_LENGTH] != 0) return 0;
    const std::size_t length = strnlen(onlineId->data, ONLINE_ID_MAX_LENGTH + 1);
    if (length < 3 || length > ONLINE_ID_MAX_LENGTH) return 0;
    if (!std::isalnum(static_cast<unsigned char>(onlineId->data[0]))) return 0;
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char character = static_cast<unsigned char>(onlineId->data[i]);
        if (!std::isalnum(character) && character != '_' && character != '-') return 0;
    }
    return 1;
}

int APS5_VABI sceNpMutexInit(PthreadMutex* mutex, const char* name, std::uint64_t flags) {
    PthreadMutexattr attr = nullptr;
    int result = scePthreadMutexattrInit(&attr);
    if (result != 0) return result;
    if (flags & 1) result = scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE);
    if (result == 0) result = scePthreadMutexInit(mutex, &attr, name);
    scePthreadMutexattrDestroy(&attr);

    return result;
}

int APS5_VABI sceNpMutexDestroy(PthreadMutex* mutex) {
    return scePthreadMutexDestroy(mutex);
}

int APS5_VABI sceNpMutexLock(PthreadMutex* mutex) {
    return scePthreadMutexLock(mutex);
}

int APS5_VABI sceNpMutexTryLock(PthreadMutex* mutex) {
    const int result = scePthreadMutexTrylock(mutex);
    return result == KERNEL_ERROR_EBUSY ? NP_LW_MUTEX_ERROR_BUSY : result;
}

int APS5_VABI sceNpMutexUnlock(PthreadMutex* mutex) {
    return scePthreadMutexUnlock(mutex);
}

int APS5_VABI sceNpLwMutexInit(PthreadMutex* mutex, const char* name, std::uint64_t flags) {
    return sceNpMutexInit(mutex, name, flags);
}

int APS5_VABI sceNpLwMutexDestroy(PthreadMutex* mutex) {
    return sceNpMutexDestroy(mutex);
}

int APS5_VABI sceNpLwMutexLock(PthreadMutex* mutex) {
    return sceNpMutexLock(mutex);
}

int APS5_VABI sceNpLwMutexTryLock(PthreadMutex* mutex) {
    return sceNpMutexTryLock(mutex);
}

int APS5_VABI sceNpLwMutexUnlock(PthreadMutex* mutex) {
    return sceNpMutexUnlock(mutex);
}

int APS5_VABI sceNpCondInit(PthreadCond* cond, const char* name, std::uint64_t) {
    PthreadCondattr attr = nullptr;
    int result = scePthreadCondattrInit(&attr);
    if (result != 0) return result;
    result = scePthreadCondInit(cond, &attr, name);
    scePthreadCondattrDestroy(&attr);

    return result;
}

int APS5_VABI sceNpCondDestroy(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI sceNpCondSignal(PthreadCond* cond) {
    return scePthreadCondSignal(cond);
}

int APS5_VABI sceNpCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, std::uint32_t usec) {
    if (usec == 0) return scePthreadCondWait(cond, mutex);
    const int result = scePthreadCondTimedwait(cond, mutex, usec);
    return result == KERNEL_ERROR_ETIMEDOUT ? NP_LW_COND_ERROR_TIMEDOUT : result;
}

int APS5_VABI sceNpJoinThread(Pthread thread, void** retval) {
    return scePthreadJoin(thread, retval);
}

int APS5_VABI sceNpGetSystemClockUsec(std::int64_t* usec) {
    KernelTimespec now{};
    const int result = sceKernelClockGettime(CLOCK_MONOTONIC_ID, &now);
    if (result == 0) *usec = now.tv_sec * 1000000 + now.tv_nsec / 1000;
    return result;
}

void APS5_VABI sceNpGetSdkVersion(char* version) {
    SceKernelSystemSwVersion swVersion{};
    swVersion.size = sizeof(swVersion);
    sceKernelGetSystemSwVersion_nid_postfix(&swVersion);

    const std::uint32_t hex = swVersion.version;
    std::snprintf(version, 8, "%u.%02u", ((hex >> 24) & 0xF) + (hex >> 28) * 10, ((hex >> 16) & 0xF) + ((hex >> 20) & 0xF) * 10);
}

void APS5_VABI sceNpGetNavSdkVersion(char* version) {
    constexpr char NAV_SDK_VERSION[8] = "00.82";
    std::memcpy(version, NAV_SDK_VERSION, sizeof(NAV_SDK_VERSION));
}

}
