#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
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

namespace UreldatefmtClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ureldatefmt_close_nid_postfix), void (APS5_VABI*)(URelativeDateTimeFormatter*)>);

void Run() {
    const auto before = NotImplementedCalls_nid_no_patch();
    ureldatefmt_close_nid_postfix(nullptr);
    ureldatefmt_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "null disposal does not hit unimplemented stub");

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    auto* foreign = reinterpret_cast<URelativeDateTimeFormatter*>(storage.data());
    bool threw = false;
    try {
        ureldatefmt_close_nid_postfix(foreign);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && NotImplementedCalls_nid_no_patch() == before + 1, "non-null handle throws NotImplemented_nid_no_patch");
    for (const auto byte : storage) {
        Require(byte == 0xa5, "non-null handle memory is untouched");
    }
}

} // namespace UreldatefmtClose

namespace UreldatefmtFormat {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ureldatefmt_format_nid_postfix), std::int32_t (APS5_VABI*)(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto destination = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));
    const auto formatter = reinterpret_cast<URelativeDateTimeFormatter*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, -3.5, 0, destination, 8, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 0.0, 7, nullptr, 4, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 0.0, 7, destination, -1, &status) == 0);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, -3.5, 0, destination, 8, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, 2.0, 5, nullptr, 0, &status); });
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 2.0, 5, nullptr, 4, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 2.0, 5, destination, -2, &status) == 0);
        Require(status == IllegalArgument);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, 0.0, 0, destination, 8, nullptr); });
}

} // namespace UreldatefmtFormat

namespace UreldatefmtOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ureldatefmt_open_nid_postfix), URelativeDateTimeFormatter* (APS5_VABI*)(const char*, UNumberFormat*, std::int32_t, std::int32_t, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto adopted = reinterpret_cast<UNumberFormat*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ureldatefmt_open_nid_postfix("en", adopted, 1, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_open_nid_postfix(nullptr, nullptr, 3, 301, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ureldatefmt_open_nid_postfix("en", adopted, 1, 0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ureldatefmt_open_nid_postfix(nullptr, nullptr, 3, 301, &status); });
        Require(status == incoming);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { ureldatefmt_open_nid_postfix("en", adopted, 1, 0, nullptr); });
}

} // namespace UreldatefmtOpen

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
    char16_t buffer[2] = {u'a', u'b'};

    rejectsRuntime([&] { ureldatefmt_format_nid_postfix(nullptr, 0.0, 0, nullptr, 1, badStatus); });
    rejectsRuntime([&] { ureldatefmt_format_nid_postfix(nullptr, 0.0, 0, nullptr, 1, roZero); });
    IcuRecoveredTest::Require(ureldatefmt_format_nid_postfix(nullptr, 0.0, 0, buffer, 2, roFailing) == 0);

    rejectsRuntime([&] { ureldatefmt_open_nid_postfix("en", nullptr, 0, 0, badStatus); });
    IcuRecoveredTest::Require(ureldatefmt_open_nid_postfix("en", nullptr, 0, 0, roFailing) == nullptr);

#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
}

}

void CheckIcuRelativeDateTimeFormatter() {
    UreldatefmtClose::Run();
    UreldatefmtFormat::Run();
    UreldatefmtOpen::Run();
    GuestPointers::Run();
}
