#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>

#include "SceTypes.hpp"
#include "HitLog.hpp"
#include "prx/libc/include/General.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;

constexpr int kErrOk = 0;
constexpr int kErrNotInitialized = static_cast<int>(0x80ED0001u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80ED0002u);
constexpr int kErrParam = static_cast<int>(0x80ED0003u);
constexpr int kErrInvalidState = static_cast<int>(0x80ED0005u);

constexpr std::size_t kParamSize = 16;

std::mutex g_lock;
int g_status = kStatusNone;

bool Readable(const void* p, std::size_t n) {
#ifdef _WIN32
    if (p == nullptr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return reinterpret_cast<std::uintptr_t>(p) + n <= end;
#else
    return p != nullptr;
#endif
}
}  // namespace

extern "C" {

int APS5_VABI sceErrorDialogInitialize(void) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusNone) return kErrAlreadyInitialized;
 g_status = kStatusInitialized;
 return kErrOk;
}

int APS5_VABI sceErrorDialogOpen(const void* param) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusInitialized && g_status != kStatusFinished) return kErrInvalidState;
 if (!Readable(param, kParamSize)) return kErrParam;
 std::int32_t size = 0;
 std::memcpy(&size, param, sizeof(size));
 if (static_cast<std::size_t>(size) != kParamSize) return kErrParam;
 std::int32_t errorCode = 0;
 std::int32_t userId = 0;
 std::memcpy(&errorCode, static_cast<const std::uint8_t*>(param) + 4, sizeof(errorCode));
 std::memcpy(&userId, static_cast<const std::uint8_t*>(param) + 8, sizeof(userId));
 APS5_HIT("ERRORDIALOG", "open: error_code=0x%08X user_id=%d", errorCode, userId);
 g_status = kStatusRunning;
 return kErrOk;
}

int APS5_VABI sceErrorDialogUpdateStatus(void) {
 std::lock_guard g(g_lock);
 if (g_status == kStatusRunning) g_status = kStatusFinished;
 return g_status;
}

int APS5_VABI sceErrorDialogGetStatus(void) {
 std::lock_guard g(g_lock);
 return g_status;
}

int APS5_VABI sceErrorDialogClose(void) {
 std::lock_guard g(g_lock);
 if (g_status != kStatusRunning) return kErrInvalidState;
 g_status = kStatusFinished;
 return kErrOk;
}

int APS5_VABI sceErrorDialogTerminate(void) {
 std::lock_guard g(g_lock);
 if (g_status == kStatusNone) return kErrNotInitialized;
 g_status = kStatusNone;
 return kErrOk;
}

}
