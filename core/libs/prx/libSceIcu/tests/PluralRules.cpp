#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string_view>
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

namespace UplrulesClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&uplrules_close_nid_postfix), void (APS5_VABI*)(UPluralRules*)>);

namespace {

void CheckNonNullThrows(UPluralRules* handle) {
    try {
        uplrules_close_nid_postfix(handle);
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()) == "uplrules_close_nid_postfix not implemented", "unexpected runtime_error message");
        return;
    }
    Require(false, "non-null handle did not throw");
}

}

void Run() {
    uplrules_close_nid_postfix(nullptr);
    uplrules_close_nid_postfix(nullptr);

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(storage.data()));
    for (const std::uint8_t byte : storage) {
        Require(byte == 0xa5, "non-null close modified caller storage");
    }
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(static_cast<std::uintptr_t>(1)));
}

} // namespace UplrulesClose

namespace UplrulesOpenForType {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&uplrules_openForType_nid_postfix), UPluralRules* (APS5_VABI*)(const char*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const std::array<std::int32_t, 3> usable{0, 1, -1};
    const std::array<std::int32_t, 3> beyond{2, 3, 0x7fffffff};

    for (const auto incoming : failing) {
        for (const auto type : usable) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == incoming);
        }

        for (const auto type : beyond) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == incoming);
        }
    }

    for (const auto incoming : passing) {
        for (const auto type : usable) {
            std::int32_t status = incoming;
            RequireThrows([&] { uplrules_openForType_nid_postfix("en", type, &status); });
            Require(status == incoming);
        }

        for (const auto type : beyond) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == IllegalArgument);
        }
    }

    RequireThrows([&] { uplrules_openForType_nid_postfix("en", 0, nullptr); });
}

} // namespace UplrulesOpenForType

namespace GuestPointers {

void Run() {
    auto rejectsRuntime = [](auto&& call) {
        errno = 71;
        bool threw = false;
        try {
            call();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        IcuRecoveredTest::Require(threw && errno == 71);
    };

    const std::size_t pageSize =
#ifdef _WIN32
        4096;
#else
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
#endif
    void* roPage = nullptr;
#ifdef _WIN32
    roPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    IcuRecoveredTest::Require(roPage != nullptr);
#else
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    IcuRecoveredTest::Require(roPage != MAP_FAILED);
#endif
    const std::int32_t zero = 0;
    const std::int32_t failing = 1;
    std::memcpy(roPage, &zero, sizeof(zero));
    std::memcpy(static_cast<char*>(roPage) + 16, &failing, sizeof(failing));
#ifdef _WIN32
    DWORD oldProtect = 0;
    IcuRecoveredTest::Require(VirtualProtect(roPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    IcuRecoveredTest::Require(mprotect(roPage, pageSize, PROT_READ) == 0);
#endif
    auto* roZero = static_cast<std::int32_t*>(roPage);
    auto* roFailing = reinterpret_cast<std::int32_t*>(static_cast<char*>(roPage) + 16);
    auto* badStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});

    rejectsRuntime([&] { uplrules_openForType_nid_postfix("en", 0, badStatus); });
    rejectsRuntime([&] { uplrules_openForType_nid_postfix("en", 2, roZero); });
    IcuRecoveredTest::Require(uplrules_openForType_nid_postfix("en", 2, roFailing) == nullptr);

#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
}

}

void CheckIcuPluralRules() {
    UplrulesClose::Run();
    UplrulesOpenForType::Run();
    GuestPointers::Run();
}
