#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
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
    std::fprintf(stderr, "ICU converter standards: %s\n", message);
    std::abort();
}

void Compare(std::uint16_t index, std::int32_t initial) {
    auto nativeStatus = static_cast<UErrorCode>(initial);
    const char* native = ucnv_getStandard(index, &nativeStatus);
    std::int32_t guestStatus = initial;
    const char* guest = ucnv_getStandard_nid_postfix(index, &guestStatus);
    Require(guestStatus == static_cast<std::int32_t>(nativeStatus), "status matches bundled ICU");
    Require((native == nullptr) == (guest == nullptr), "null result matches bundled ICU");
    if (native) Require(std::strcmp(guest, native) == 0, "standard name matches bundled ICU");
}

template<class TCall>
void RequiresInvalidArgument(TCall call, const char* message) {
    bool threw = false;
    try {
        call();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, message);
}

template<class TCall>
void RequiresRuntimeErrorKeepingErrno(TCall call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void Parity() {
    static_assert(std::is_same_v<decltype(&ucnv_getStandard_nid_postfix),
                                 const char* (APS5_VABI*)(std::uint16_t, std::int32_t*)>);
    const auto count = ucnv_countStandards();
    const std::array<std::uint16_t, 5> indices{
        0,
        1,
        static_cast<std::uint16_t>(count > 0 ? count - 1 : 0),
        static_cast<std::uint16_t>(count),
        std::numeric_limits<std::uint16_t>::max(),
    };
    for (const auto index : indices) for (const std::int32_t initial : {0, -1, -128}) Compare(index, initial);
    for (const std::int32_t initial : {1, 15, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = initial;
        Require(!ucnv_getStandard_nid_postfix(0, &status), "preexisting failure returns null");
        Require(status == initial, "preexisting failure remains unchanged");
    }
    if (count > 0) {
        std::int32_t status = 0;
        const char* first = ucnv_getStandard_nid_postfix(0, &status);
        Require(first && status <= 0, "available standard succeeds");
        std::int32_t repeatedStatus = 0;
        Require(ucnv_getStandard_nid_postfix(0, &repeatedStatus) == first && repeatedStatus == status,
                "standard name has stable storage");
    }
    RequiresInvalidArgument([&] { ucnv_getStandard_nid_postfix(0, nullptr); }, "null status throws");
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* page = static_cast<char*>(VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(page != nullptr, "allocate guest pointer page");
#else
    const long configuredPageSize = sysconf(_SC_PAGESIZE);
    const std::size_t pageSize = static_cast<std::size_t>(configuredPageSize > 0 ? configuredPageSize : 4096);
    void* mapping = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pointer page");
    auto* page = static_cast<char*>(mapping);
#endif
    const std::int32_t zero = 0;
    const std::int32_t failing = 1;
    std::memcpy(page, &zero, sizeof(zero));
    std::memcpy(page + 16, &failing, sizeof(failing));
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect status page");
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0, "protect status page");
#endif
    auto* readOnlyZero = reinterpret_cast<std::int32_t*>(page);
    auto* readOnlyFailing = reinterpret_cast<std::int32_t*>(page + 16);
    auto* unreadable = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_getStandard_nid_postfix(0, unreadable); },
                                     "unreadable status throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_getStandard_nid_postfix(0, readOnlyZero); },
                                     "unwritable active status throws and keeps errno");
    Require(!ucnv_getStandard_nid_postfix(0, readOnlyFailing), "read-only failing status returns immediately");
    Require(*readOnlyFailing == failing, "read-only failing status remains unchanged");
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
}

}

int main() {
    Parity();
    GuestPointers();
}
