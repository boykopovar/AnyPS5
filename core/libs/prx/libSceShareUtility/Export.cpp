#include "prx/libSceShareUtility/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotSupported = static_cast<int>(0x81170001u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x81170003u);
constexpr int ErrorInvalidSize = static_cast<int>(0x81170004u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x81170101u);
constexpr int ErrorNotInitialized = static_cast<int>(0x81170102u);

constexpr std::uint64_t MinHeapSize = 0x20000u;
constexpr std::uint64_t MaxUploadSize = 0x1fffu;
constexpr std::uint64_t MaxShareContentSize = 0x1000u;

std::mutex g_mutex;
bool g_basicInitialized = false;
bool g_advanceInitialized = false;
std::int64_t g_playingContent = -1;
std::vector<std::uint8_t> g_uploadContentData;

bool ValidExParams(std::int32_t priority, const void* affinity) {
    return priority >= 0x100 && priority <= 0x2ff && affinity != nullptr;
}

}

extern "C" {

int APS5_VABI sceShareUtilityAdvanceFromFileWithContentIdList() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceFromFileWithTitleIdList() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceGetServiceInfo() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceInitialize(std::uint64_t argument1, std::uint64_t argument2, std::int32_t priority, const void* affinity) {
    if (argument1 != 2 || argument2 < MinHeapSize) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (g_advanceInitialized) return ErrorAlreadyInitialized;
    if (!ValidExParams(priority, affinity)) return ErrorInvalidArgument;
    g_advanceInitialized = true;
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfo() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfoA() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceResetPlayingContent() {
    std::lock_guard lock(g_mutex);
    if (!g_advanceInitialized) return ErrorNotInitialized;
    g_playingContent = -1;
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceSetPlayingContent(std::int64_t argument1) {
    if (argument1 < 0) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (!g_advanceInitialized) return ErrorNotInitialized;
    g_playingContent = argument1;
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceSetUploadContentData(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return ErrorInvalidArgument;
    if (argument2 > MaxUploadSize) return ErrorInvalidSize;
    std::vector<std::uint8_t> copied(static_cast<std::size_t>(argument2));
    const int savedErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(copied.data(), argument1, copied.size());
    errno = savedErrno;
    if (!readOk) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (!g_advanceInitialized) return ErrorNotInitialized;
    g_uploadContentData = std::move(copied);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceTerminate() {
    std::lock_guard lock(g_mutex);
    if (!g_advanceInitialized) return ErrorNotInitialized;
    g_advanceInitialized = false;
    g_playingContent = -1;
    g_uploadContentData.clear();
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceValidateContents() {
    return ErrorNotSupported;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenu(std::int64_t argument1, std::uint32_t argument2) {
    if (argument2 < 1 || argument2 > 4 || argument1 < 0) return ErrorInvalidArgument;
    {
        std::lock_guard lock(g_mutex);
        if (!g_advanceInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuBroadcast() {
    {
        std::lock_guard lock(g_mutex);
        if (!g_advanceInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuDefault() {
    {
        std::lock_guard lock(g_mutex);
        if (!g_advanceInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuForShareContent(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return ErrorInvalidArgument;
    if (argument2 >= MaxShareContentSize) return ErrorInvalidSize;
    {
        std::lock_guard lock(g_mutex);
        if (!g_advanceInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityInitialize(std::uint64_t argument1, std::uint64_t argument2) {
    if (argument1 != 1 || argument2 < MinHeapSize) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (g_basicInitialized) return ErrorAlreadyInitialized;
    g_basicInitialized = true;
    return 0;
}

int APS5_VABI sceShareUtilityInitializeEx2(std::uint64_t argument1, std::uint64_t argument2, std::int32_t priority, const void* affinity) {
    if (argument1 != 1 || argument2 < MinHeapSize) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (g_basicInitialized) return ErrorAlreadyInitialized;
    if (!ValidExParams(priority, affinity)) return ErrorInvalidArgument;
    g_basicInitialized = true;
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenu(std::int64_t argument1, std::uint32_t argument2) {
    if (argument2 < 1 || argument2 > 4 || argument1 < 0) return ErrorInvalidArgument;
    {
        std::lock_guard lock(g_mutex);
        if (!g_basicInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuBroadcast() {
    {
        std::lock_guard lock(g_mutex);
        if (!g_basicInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuDefault() {
    {
        std::lock_guard lock(g_mutex);
        if (!g_basicInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuForShareContent(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return ErrorInvalidArgument;
    if (argument2 >= MaxShareContentSize) return ErrorInvalidSize;
    {
        std::lock_guard lock(g_mutex);
        if (!g_basicInitialized) return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityTerminate() {
    std::lock_guard lock(g_mutex);
    if (!g_basicInitialized) return ErrorNotInitialized;
    g_basicInitialized = false;
    return 0;
}

}
