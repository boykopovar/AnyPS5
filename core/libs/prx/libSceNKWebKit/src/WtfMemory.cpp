#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfMemoryTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void* TryAllocate(std::size_t size, std::size_t alignment = 16);
void* Reallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void* TryReallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void Deallocate(void* ptr);
std::size_t AllocationSize(const void* ptr);
}  // namespace NKWebKit

namespace {

void StoreTryMallocResult(WTF::TryMallocReturnValue* result, void* data, const char* func) {
    if (GuestCopyOut_nid_no_patch(result, &data, sizeof(data))) return;
    NKWebKit::Deallocate(data);
    NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
}

bool CopyGuestCString(const char* source, std::string& destination) {
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    for (std::size_t offset = 0;;) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - base) return false;
        const auto current = base + offset;
        const std::size_t chunk = std::min<std::size_t>(256, 4096 - current % 4096);
        char buffer[256];
        if (!GuestCopyIn_nid_no_patch(buffer, reinterpret_cast<const void*>(current), chunk)) return false;
        if (const auto* end = static_cast<const char*>(std::memchr(buffer, 0, chunk))) {
            destination.append(buffer, static_cast<std::size_t>(end - buffer));
            return true;
        }
        destination.append(buffer, chunk);
        offset += chunk;
    }
}

}

extern "C" {
void* APS5_VABI _ZN3WTF10fastMallocEm(std::size_t size) {
    return NKWebKit::Allocate(size);
}

void APS5_VABI _ZN3WTF8fastFreeEPv(void* ptr) {
    NKWebKit::Deallocate(ptr);
}

void* APS5_VABI _ZN7WebCore9GLContextnwEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void* APS5_VABI _ZN7WebCore9GLContextnaEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void APS5_VABI _ZN7WebCore9GLContextdlEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void APS5_VABI _ZN7WebCore9GLContextdaEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void* APS5_VABI _ZN7WebCore9GLContextnwEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN7WebCore9GLContextnaEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN7WebCore17LibWebRTCProvidernwEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void* APS5_VABI _ZN7WebCore17LibWebRTCProvidernaEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderdlEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void APS5_VABI _ZN7WebCore17LibWebRTCProviderdaEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void* APS5_VABI _ZN7WebCore17LibWebRTCProvidernwEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN7WebCore17LibWebRTCProvidernaEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN7WebCore6ISOBoxnwEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void* APS5_VABI _ZN7WebCore6ISOBoxnaEm(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void APS5_VABI _ZN7WebCore6ISOBoxdlEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void APS5_VABI _ZN7WebCore6ISOBoxdaEPv(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

void* APS5_VABI _ZN7WebCore6ISOBoxnwEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN7WebCore6ISOBoxnaEmPv(std::size_t, void* location) {
    return location;
}

void* APS5_VABI _ZN3WTF10fastCallocEmm(std::size_t count, std::size_t size) {
    if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
        std::abort();
    }
    const std::size_t total = count * size;
    void* ptr = NKWebKit::Allocate(total);
    std::memset(ptr, 0, total);
    return ptr;
}

void* APS5_VABI _ZN3WTF11fastReallocEPvm(void* ptr, std::size_t size) {
    return NKWebKit::Reallocate(ptr, size);
}

void* APS5_VABI _ZN3WTF16fastZeroedMallocEm(std::size_t size) {
    void* ptr = NKWebKit::Allocate(size);
    std::memset(ptr, 0, size);
    return ptr;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF13tryFastMallocEm(WTF::TryMallocReturnValue* result, std::size_t size) {
    StoreTryMallocResult(result, NKWebKit::TryAllocate(size), __func__);
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF19tryFastZeroedMallocEm(WTF::TryMallocReturnValue* result, std::size_t size) {
    void* ptr = NKWebKit::TryAllocate(size);
    if (ptr) {
        std::memset(ptr, 0, size);
    }
    StoreTryMallocResult(result, ptr, __func__);
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF13tryFastCallocEmm(WTF::TryMallocReturnValue* result, std::size_t count, std::size_t size) {
    if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
        StoreTryMallocResult(result, nullptr, __func__);
        return result;
    }
    const std::size_t total = count * size;
    void* ptr = NKWebKit::TryAllocate(total);
    if (ptr) {
        std::memset(ptr, 0, total);
    }
    StoreTryMallocResult(result, ptr, __func__);
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF14tryFastReallocEPvm(WTF::TryMallocReturnValue* result, void* ptr, std::size_t size) {
    if (!GuestWritable_nid_no_patch(result, sizeof(*result))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    void* data = NKWebKit::TryReallocate(ptr, size);
    if (!GuestCopyOut_nid_no_patch(result, &data, sizeof(data))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return result;
}

void* APS5_VABI _ZN3WTF17fastAlignedMallocEmm(std::size_t alignment, std::size_t size) {
    return NKWebKit::Allocate(size, alignment);
}

void* APS5_VABI _ZN3WTF20tryFastAlignedMallocEmm(std::size_t alignment, std::size_t size) {
    return NKWebKit::TryAllocate(size, alignment);
}

void APS5_VABI _ZN3WTF15fastAlignedFreeEPv(void* ptr) {
    NKWebKit::Deallocate(ptr);
}

std::size_t APS5_VABI _ZN3WTF14fastMallocSizeEPKv(const void* ptr) {
    return NKWebKit::AllocationSize(ptr);
}

std::size_t APS5_VABI _ZN3WTF18fastMallocGoodSizeEm(std::size_t size) {
    return size;
}

void APS5_VABI _ZN3WTF27releaseFastMallocFreeMemoryEv() {
}

void APS5_VABI _ZN3WTF40releaseFastMallocFreeMemoryForThisThreadEv() {
}

char* APS5_VABI _ZN3WTF10fastStrDupEPKc(const char* str) {
    if (!str) {
        return nullptr;
    }
    std::string copy;
    if (!CopyGuestCString(str, copy)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto* dup = static_cast<char*>(NKWebKit::Allocate(copy.size() + 1));
    std::memcpy(dup, copy.c_str(), copy.size() + 1);
    return dup;
}

bool APS5_VABI _ZN3WTF19isFastMallocEnabledEv() {
    return true;
}

void APS5_VABI _ZN3WTF23fastCommitAlignedMemoryEPvm(void*, std::size_t) {
}

void APS5_VABI _ZN3WTF25fastDecommitAlignedMemoryEPvm(void*, std::size_t) {
}

void APS5_VABI _ZN3WTF18fastEnableMiniModeEv() {
}

void APS5_VABI _ZN3WTF20fastDisableScavengerEv() {
}

FastMallocStatistics APS5_VABI _ZN3WTF20fastMallocStatisticsEv() {
    FastMallocStatistics stats{};
    return stats;
}

void APS5_VABI _ZN3WTF25fastMallocDumpMallocStatsEv() {
}

void* APS5_VABI _ZN3WTF13jsValueMallocEm(std::size_t size) {
    return NKWebKit::Allocate(size);
}

void* APS5_VABI _ZN3WTF16tryJSValueMallocEm(std::size_t size) {
    return NKWebKit::TryAllocate(size);
}

void APS5_VABI _ZN3WTF11jsValueFreeEPv(void* ptr) {
    NKWebKit::Deallocate(ptr);
}

void* APS5_VABI _ZN3WTF14jsValueReallocEPvm(void* ptr, std::size_t size) {
    return NKWebKit::Reallocate(ptr, size);
}

}
