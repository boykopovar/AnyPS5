#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfMemoryTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void* TryAllocate(std::size_t size, std::size_t alignment = 16);
void* Reallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void* TryReallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void Deallocate(void* ptr);
std::size_t AllocationSize(const void* ptr);
}  // namespace NKWebKit

extern "C" {
void* APS5_VABI _ZN3WTF10fastMallocEm(std::size_t size) {
    return NKWebKit::Allocate(size);
}

void APS5_VABI _ZN3WTF8fastFreeEPv(void* ptr) {
    NKWebKit::Deallocate(ptr);
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
    result->data = NKWebKit::TryAllocate(size);
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF19tryFastZeroedMallocEm(WTF::TryMallocReturnValue* result, std::size_t size) {
    void* ptr = NKWebKit::TryAllocate(size);
    if (ptr) {
        std::memset(ptr, 0, size);
    }
    result->data = ptr;
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF13tryFastCallocEmm(WTF::TryMallocReturnValue* result, std::size_t count, std::size_t size) {
    if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
        result->data = nullptr;
        return result;
    }
    const std::size_t total = count * size;
    void* ptr = NKWebKit::TryAllocate(total);
    if (ptr) {
        std::memset(ptr, 0, total);
    }
    result->data = ptr;
    return result;
}

WTF::TryMallocReturnValue* APS5_VABI _ZN3WTF14tryFastReallocEPvm(WTF::TryMallocReturnValue* result, void* ptr, std::size_t size) {
    result->data = NKWebKit::TryReallocate(ptr, size);
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
    const std::size_t len = std::strlen(str);
    auto* dup = static_cast<char*>(NKWebKit::Allocate(len + 1));
    std::memcpy(dup, str, len + 1);
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
