#include "prx/libSceNpSessionSignalingCrossplay/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x80553D01u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80553D02u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80553D03u);
constexpr int ErrorHeapInit = static_cast<int>(0x80553D05u);

std::mutex g_mutex;
bool g_initialized = false;
NpMemoryHeap g_heap{};

static_assert(sizeof(NpSessionSignalingCrossplayInitParam) == 0x28);
static_assert(offsetof(NpSessionSignalingCrossplayInitParam, reserved0) == 0x00);
static_assert(offsetof(NpSessionSignalingCrossplayInitParam, heapSize) == 0x08);
static_assert(offsetof(NpSessionSignalingCrossplayInitParam, reserved1) == 0x10);
static_assert(offsetof(NpSessionSignalingCrossplayInitParam, reserved2) == 0x18);
static_assert(offsetof(NpSessionSignalingCrossplayInitParam, reserved3) == 0x20);
static_assert(sizeof(NpSessionSignalingCrossplayMemoryInfo) == 0x18);
static_assert(offsetof(NpSessionSignalingCrossplayMemoryInfo, poolSize) == 0x00);
static_assert(offsetof(NpSessionSignalingCrossplayMemoryInfo, currentInuseSize) == 0x08);
static_assert(offsetof(NpSessionSignalingCrossplayMemoryInfo, maxInuseSize) == 0x10);

struct ErrnoGuard {
    int savedErrno = errno;

    ErrnoGuard() = default;
    ErrnoGuard(const ErrnoGuard&) = delete;
    ErrnoGuard& operator=(const ErrnoGuard&) = delete;
    ~ErrnoGuard() {
        errno = savedErrno;
    }
};

template <typename T>
bool CopyGuestIn(T* destination, const T* source) {
    const ErrnoGuard errnoGuard;
    return GuestCopyIn_nid_no_patch(destination, source, sizeof(T));
}

template <typename T>
bool CopyGuestOut(T* destination, const T& source) {
    const ErrnoGuard errnoGuard;
    return GuestCopyOut_nid_no_patch(destination, &source, sizeof(T));
}

} // namespace

extern "C" {

int APS5_VABI sceNpSessionSignalingCrossplayInitialize(const NpSessionSignalingCrossplayInitParam* parameter) {
    if (parameter == nullptr) return ErrorInvalidArgument;
    const ErrnoGuard errnoGuard;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return ErrorAlreadyInitialized;
    NpSessionSignalingCrossplayInitParam localParam{};
    if (!CopyGuestIn(&localParam, parameter)) return ErrorInvalidArgument;
    if (localParam.heapSize == 0) return ErrorInvalidArgument;
    if (sceNpMemoryHeapInit(&g_heap, localParam.heapSize, "SceNpSessionSignaling") < 0) return ErrorHeapInit;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayTerminate(void) {
    const ErrnoGuard errnoGuard;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    sceNpMemoryHeapDestroy(&g_heap);
    g_heap = {};
    g_initialized = false;
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayCreateContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("SPHdC4zuXfY", sceNpSessionSignalingCrossplayUnknown00);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown00(void) {
    NotImplemented_nid_no_patch("SPHdC4zuXfY");
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayDestroyContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("2nvt1vCR3qo", sceNpSessionSignalingCrossplayUnknown01);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown01(void) {
    NotImplemented_nid_no_patch("2nvt1vCR3qo");
    return 0;
}

APS5_EXPORT("wypjyjGWaLM", sceNpSessionSignalingCrossplayUnknown02);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown02(std::uint64_t argument1, const void* argument2, std::uint64_t argument3, const void* argument4, void* argument5) {
    if (argument2 == nullptr || argument4 == nullptr || argument5 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch("wypjyjGWaLM");
    return 0;
}

APS5_EXPORT("eGv1EEmON+A", sceNpSessionSignalingCrossplayUnknown03);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown03(void) {
    NotImplemented_nid_no_patch("eGv1EEmON+A");
    return 0;
}

APS5_EXPORT("GMGOCumydk0", sceNpSessionSignalingCrossplayUnknown04);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown04(void) {
    NotImplemented_nid_no_patch("GMGOCumydk0");
    return 0;
}

APS5_EXPORT("LfFgtyCjdbs", sceNpSessionSignalingCrossplayUnknown05);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown05(void) {
    NotImplemented_nid_no_patch("LfFgtyCjdbs");
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayGetMemoryInfo(NpSessionSignalingCrossplayMemoryInfo* info) {
    if (info == nullptr) return ErrorInvalidArgument;
    const ErrnoGuard errnoGuard;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    NpHeapStat stat{};
    if (sceNpHeapGetStat(&g_heap.heap, &stat) != 0) return ErrorInvalidArgument;
    const NpSessionSignalingCrossplayMemoryInfo localInfo{
        stat.maxSystemSize,
        stat.currentInuseSize,
        stat.maxInuseSize,
    };
    if (!CopyGuestOut(info, localInfo)) return ErrorInvalidArgument;
    return 0;
}

}
