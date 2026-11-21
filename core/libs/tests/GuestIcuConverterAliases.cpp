#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
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
    std::fprintf(stderr, "ICU converter aliases: %s\n", message);
    std::abort();
}

template<class TCall>
void RequireRuntimeError(TCall call, const char* message) {
    errno = 71;
    try {
        call();
    } catch (const std::runtime_error&) {
        Require(errno == 71, "guest pointer failure preserves errno");
        return;
    }
    Require(false, message);
}

template<class TCall>
void RequireInvalidArgument(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

std::string CheckedString(const char* source) {
    std::string result;
    for (;;) {
        char value = 0;
        Require(GuestCopyIn_nid_no_patch(&value, source + result.size(), sizeof(value)),
                "borrowed alias pointer is guest-readable");
        if (!value) return result;
        result.push_back(value);
    }
}

void CheckParity(const char* alias, std::int32_t initial) {
    UErrorCode nativeStatus = static_cast<UErrorCode>(initial);
    const auto count = ucnv_countAliases(alias, &nativeStatus);
    std::vector<const char*> expected(count);
    if (U_SUCCESS(nativeStatus) && count != 0) ucnv_getAliases(alias, expected.data(), &nativeStatus);

    std::vector<const char*> actual(static_cast<std::size_t>(count) + 2,
                                    reinterpret_cast<const char*>(std::uintptr_t{0x1234}));
    std::int32_t guestStatus = initial;
    ucnv_getAliases_nid_postfix(alias, actual.data() + 1, &guestStatus);
    Require(guestStatus == static_cast<std::int32_t>(nativeStatus), "status matches pinned ICU");
    Require(actual.front() == reinterpret_cast<const char*>(std::uintptr_t{0x1234}) &&
            actual.back() == reinterpret_cast<const char*>(std::uintptr_t{0x1234}), "output canaries remain intact");
    for (std::size_t index = 0; index < count; ++index) {
        Require(CheckedString(actual[index + 1]) == expected[index], "alias value and order match pinned ICU");
        Require(CheckedString(expected[index]) == expected[index], "native alias storage is guest-readable");
    }
}

void Parity() {
    static_assert(std::is_same_v<decltype(&ucnv_getAliases_nid_postfix),
                  void (APS5_VABI*)(const char*, const char**, std::int32_t*)>);
    const std::string shortLimit(UCNV_MAX_CONVERTER_NAME_LENGTH - 1, 'a');
    const std::string exactLimit(UCNV_MAX_CONVERTER_NAME_LENGTH, 'a');
    const std::string pastLimit(UCNV_MAX_CONVERTER_NAME_LENGTH + 1, 'a');
    for (std::int32_t status : {0, -1, -128, 1, 15}) {
        CheckParity(nullptr, status);
        CheckParity("", status);
        CheckParity("UTF-8", status);
        CheckParity("u*T@f08", status);
        CheckParity("not-a-converter", status);
        CheckParity(shortLimit.c_str(), status);
        CheckParity(exactLimit.c_str(), status);
        CheckParity(pastLimit.c_str(), status);
    }
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate guest pages");
#else
    const long configured = sysconf(_SC_PAGESIZE);
    const std::size_t pageSize = static_cast<std::size_t>(configured > 0 ? configured : 4096);
    void* mapping = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    const std::int32_t success = 0;
    const std::int32_t failure = 1;
    std::memcpy(pages, &success, sizeof(success));
    std::memcpy(pages + 16, &failure, sizeof(failure));
    std::memcpy(pages + pageSize - 6, "UTF-8", 6);
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &previous) != 0, "protect input page");
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect guard page");
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0, "protect input page");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
    auto* readOnlySuccess = reinterpret_cast<std::int32_t*>(pages);
    auto* readOnlyFailure = reinterpret_cast<std::int32_t*>(pages + 16);
    auto* invalidStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto* invalidAlias = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto** invalidOutput = reinterpret_cast<const char**>(std::uintptr_t{0x10});

    RequireInvalidArgument([&] { ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, nullptr); },
                           "null status throws before other pointers are read");
    RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", nullptr, invalidStatus); },
                        "unreadable status throws");
    ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, readOnlyFailure);
    Require(*readOnlyFailure == failure, "prefailed read-only status returns before other pointers are read");
    RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", nullptr, readOnlySuccess); },
                        "successful read-only status is rejected before ICU work");

    UErrorCode probeStatus = U_ZERO_ERROR;
    const auto count = ucnv_countAliases("UTF-8", &probeStatus);
    std::int32_t boundaryStatus = 0;
    std::vector<const char*> aliases(static_cast<std::size_t>(count) + 2,
                                     reinterpret_cast<const char*>(std::uintptr_t{0x5678}));
    ucnv_getAliases_nid_postfix(pages + pageSize - 6, aliases.data() + 1, &boundaryStatus);
    Require(boundaryStatus == static_cast<std::int32_t>(probeStatus), "page-boundary alias status matches native");
    if (U_SUCCESS(probeStatus) && count != 0) {
        RequireRuntimeError([&] { ucnv_getAliases_nid_postfix(invalidAlias, aliases.data() + 1, &boundaryStatus); },
                            "unreadable alias throws after alias data is available");
        boundaryStatus = 0;
        RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", invalidOutput, &boundaryStatus); },
                            "unwritable alias array throws without a partial write");
    } else {
        boundaryStatus = 0;
        ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, &boundaryStatus);
        Require(boundaryStatus == static_cast<std::int32_t>(probeStatus),
                "alias-data failure precedes input and output access");
    }

#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 3);
#endif
}

}

int main() {
    Parity();
    GuestPointers();
}
