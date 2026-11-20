#include "prx/libSceIcu/HostEnumeration.hpp"
#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "unicode/ucnv.h"
#include "unicode/uenum.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter available names: %s\n", message);
    std::abort();
}

template<class TCall>
void RejectsInvalid(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* result = nullptr;
    return posix_memalign(&result, alignment, size) == 0 ? result : nullptr;
#endif
}
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t alignment) {
#ifdef _WIN32
    return _aligned_realloc(pointer, size, alignment);
#else
    void* result = Align(alignment, size);
    std::free(pointer);
    return result;
#endif
}
int APS5_VABI PosixAlign(void** pointer, std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    *pointer = _aligned_malloc(size, alignment);
    return *pointer ? 0 : 12;
#else
    return posix_memalign(pointer, alignment, size);
#endif
}

void CompareEnumerations(UEnumeration* host, UEnumeration* guest, UErrorCode& hostStatus, std::int32_t& guestStatus) {
    const auto hostCount = uenum_count(host, &hostStatus);
    const auto guestCount = uenum_count_nid_postfix(guest, &guestStatus);
    Require(hostCount == guestCount && static_cast<std::int32_t>(hostStatus) == guestStatus, "count matches host ICU");
    for (;;) {
        std::int32_t hostLength = -1;
        std::int32_t guestLength = -1;
        const char* hostName = uenum_next(host, &hostLength, &hostStatus);
        const char* guestName = uenum_next_nid_postfix(guest, &guestLength, &guestStatus);
        Require((hostName == nullptr) == (guestName == nullptr), "iteration ends with host ICU");
        Require(hostLength == guestLength && static_cast<std::int32_t>(hostStatus) == guestStatus, "iteration metadata matches host ICU");
        if (!hostName) break;
        Require(std::strcmp(hostName, guestName) == 0, "converter name matches host ICU");
    }
}

void ProductionParity() {
    UErrorCode hostStatus = U_ZERO_ERROR;
    UEnumeration* host = ucnv_openAllNames(&hostStatus);
    std::int32_t guestStatus = 0;
    UEnumeration* guest = ucnv_openAllNames_nid_postfix(&guestStatus);
    Require((host == nullptr) == (guest == nullptr), "availability matches reduced host data");
    Require(static_cast<std::int32_t>(hostStatus) == guestStatus, "open status matches reduced host data");
    if (host) CompareEnumerations(host, guest, hostStatus, guestStatus);
    uenum_close(host);
    uenum_close_nid_postfix(guest);
    for (std::int32_t initial : {1, 15, INT32_MAX}) {
        guestStatus = initial;
        Require(!ucnv_openAllNames_nid_postfix(&guestStatus) && guestStatus == initial, "prefailed status is preserved");
    }
    RejectsInvalid([] { ucnv_openAllNames_nid_postfix(nullptr); }, "null status is rejected");
}

void AdapterLifecycle() {
    constexpr std::array<const char*, 3> words{"alpha", "be", ""};
    std::int32_t status = -1;
    UEnumeration* enumeration = Icu::OpenHostEnumeration(
        &status,
        [&](UErrorCode* local) { return uenum_openCharStringsEnumeration(words.data(), static_cast<std::int32_t>(words.size()), local); },
        "AdapterLifecycle");
    Require(enumeration && status == -1, "adapter opens an ICU enumeration and preserves warnings");
    Require(!enumeration->baseContext && enumeration->context && enumeration->close && enumeration->count &&
            enumeration->uNext && enumeration->next && enumeration->reset,
            "adapter provides the guest enumeration record");
    Require(uenum_count_nid_postfix(enumeration, &status) == 3 && status == -1, "guest count dispatches to ICU");
    for (const char* expected : words) {
        std::int32_t length = -1;
        const char* value = uenum_next_nid_postfix(enumeration, &length, &status);
        Require(value && std::strcmp(value, expected) == 0 && length == static_cast<std::int32_t>(std::strlen(expected)),
                "guest next dispatches to ICU");
    }
    std::int32_t length = -1;
    Require(!uenum_next_nid_postfix(enumeration, &length, &status) && length == 0, "guest next reports exhaustion");
    enumeration->reset(enumeration, &status);
    Require(status == -1, "guest reset preserves warning status");
    const char16_t* value = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(value && std::char_traits<char16_t>::compare(value, u"alpha", 6) == 0 && length == 5,
            "guest unext dispatches through the host conversion buffer");
    enumeration->reset(enumeration, &status);
    Require(uenum_next_nid_postfix(enumeration, nullptr, &status) != nullptr, "guest next accepts a null length");
    uenum_close_nid_postfix(enumeration);
}

void GuestStatusMemory() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate status pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate status pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    const std::int32_t success = 0;
    const std::int32_t failure = 1;
    std::memcpy(pages, &success, sizeof(success));
    std::memcpy(pages + 16, &failure, sizeof(failure));
#ifdef _WIN32
    DWORD oldProtection = 0;
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &oldProtection) != 0, "protect read-only status page");
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0, "protect unreadable status page");
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0, "protect read-only status page");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect unreadable status page");
#endif
    auto* readOnlySuccess = reinterpret_cast<std::int32_t*>(pages);
    auto* readOnlyFailure = reinterpret_cast<std::int32_t*>(pages + 16);
    auto* unreadable = reinterpret_cast<std::int32_t*>(pages + pageSize);
    Require(!ucnv_openAllNames_nid_postfix(readOnlyFailure), "read-only prefailed status is not written");
    errno = 71;
    RejectsInvalid([&] { ucnv_openAllNames_nid_postfix(unreadable); }, "unreadable status is rejected");
    Require(errno == 71, "unreadable status keeps errno");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UEnumeration* host = ucnv_openAllNames(&hostStatus);
    uenum_close(host);
    if (hostStatus == U_ZERO_ERROR) {
        UEnumeration* guest = ucnv_openAllNames_nid_postfix(readOnlySuccess);
        Require(guest != nullptr, "read-only unchanged success status is accepted");
        uenum_close_nid_postfix(guest);
    } else {
        errno = 71;
        RejectsInvalid([&] { ucnv_openAllNames_nid_postfix(readOnlySuccess); }, "read-only changed failure status is rejected");
        Require(errno == 71, "read-only changed status keeps errno");
    }
#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    ProductionParity();
    AdapterLifecycle();
    GuestStatusMemory();
    return 0;
}
