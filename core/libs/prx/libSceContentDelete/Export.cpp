#include "prx/libSceContentDelete/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};

constexpr int InvalidArgument = static_cast<int>(0x809D5001u);
constexpr int NotInitialized = static_cast<int>(0x809D5002u);
constexpr int AlreadyInitialized = static_cast<int>(0x809D5003u);
}

extern "C" {

int APS5_VABI sceContentDeleteInitialize(const ContentDeleteInitParam* initParam) {
    if (initParam == nullptr) return InvalidArgument;
    ContentDeleteInitParam localParam{};
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&localParam, initParam, sizeof(localParam));
    errno = savedErrno;
    if (!read || localParam.heap_size != 0x4000) return InvalidArgument;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return AlreadyInitialized;
    return 0;
}

int APS5_VABI sceContentDeleteByPath(const char* contentPath) {
    if (!g_initialized.load()) return NotInitialized;
    if (contentPath == nullptr) APS5_INVALID_ARG_EX;
    char firstChar = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&firstChar, contentPath, sizeof(firstChar));
    errno = savedErrno;
    if (!copied || firstChar == '\0') return InvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteById(std::int64_t contentId) {
    if (!g_initialized.load()) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteByIdListForSf(const std::int64_t* contentIds, std::size_t count) {
    if (!g_initialized.load()) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteGlobalMstat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteMemoryInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteMemoryTerminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentDeleteTerminate(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) return NotInitialized;
    return 0;
}

}
