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

namespace UfmtvalGetString {


static_assert(std::is_same_v<decltype(&ufmtval_getString_nid_postfix), const char16_t* (APS5_VABI*)(const UFormattedValue*, std::int32_t*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto storage = Filler();
    const auto* value = reinterpret_cast<const UFormattedValue*>(storage.data());

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        std::int32_t length = 0x5a5a5a5a;
        Require(ufmtval_getString_nid_postfix(nullptr, &length, &status) == nullptr);
        Require(status == incoming);
        Require(length == 0x5a5a5a5a);

        status = incoming;
        length = 0x5a5a5a5a;
        Require(ufmtval_getString_nid_postfix(value, &length, &status) == nullptr);
        Require(status == incoming);
        Require(length == 0x5a5a5a5a);
        Require(Unchanged(storage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        std::int32_t length = 0x5a5a5a5a;
        Require(ufmtval_getString_nid_postfix(nullptr, &length, &status) == nullptr);
        Require(status == IllegalArgument);
        Require(length == 0x5a5a5a5a);
    }

    ExpectErr([&] { ufmtval_getString_nid_postfix(nullptr, nullptr, nullptr); });

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        std::int32_t length = 0x5a5a5a5a;
        ExpectErr([&] { ufmtval_getString_nid_postfix(value, &length, &status); });
        Require(status == incoming);
        Require(length == 0x5a5a5a5a);
        Require(Unchanged(storage));
    }
}

} // namespace UfmtvalGetString

namespace UfmtvalNextPosition {


static_assert(std::is_same_v<decltype(&ufmtval_nextPosition_nid_postfix), std::int8_t (APS5_VABI*)(const UFormattedValue*, UConstrainedFieldPosition*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto valueStorage = Filler();
    auto positionStorage = Filler();
    const auto* value = reinterpret_cast<const UFormattedValue*>(valueStorage.data());
    auto* position = reinterpret_cast<UConstrainedFieldPosition*>(positionStorage.data());

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(nullptr, nullptr, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(value, position, &status) == 0);
        Require(status == incoming);
        Require(Unchanged(valueStorage) && Unchanged(positionStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(nullptr, position, &status) == 0);
        Require(status == IllegalArgument);
        Require(Unchanged(positionStorage));

        status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(value, nullptr, &status) == 0);
        Require(status == IllegalArgument);
        Require(Unchanged(valueStorage));
    }

    ExpectErr([&] { ufmtval_nextPosition_nid_postfix(nullptr, nullptr, nullptr); });

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        ExpectErr([&] { ufmtval_nextPosition_nid_postfix(value, position, &status); });
        Require(status == incoming);
        Require(Unchanged(valueStorage) && Unchanged(positionStorage));
    }
}

} // namespace UfmtvalNextPosition

namespace FormattedValueGuestPointers {

void Run() {
    auto rejectsRuntime = [](auto&& call) {
        errno = 71;
        bool threw = false;
        try {
            call();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && errno == 71);
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
    Require(roPage != nullptr);
#else
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roPage != MAP_FAILED);
#endif
    const std::int32_t zero = 0;
    const std::int32_t failing = 1;
    std::memcpy(roPage, &zero, sizeof(zero));
    std::memcpy(static_cast<char*>(roPage) + 16, &failing, sizeof(failing));
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(roPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    Require(mprotect(roPage, pageSize, PROT_READ) == 0);
#endif
    auto* roZero = static_cast<std::int32_t*>(roPage);
    auto* roFailing = reinterpret_cast<std::int32_t*>(static_cast<char*>(roPage) + 16);
    auto* badStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto dummyValue = reinterpret_cast<const UFormattedValue*>(roPage);

    rejectsRuntime([&] { ufmtval_getString_nid_postfix(nullptr, nullptr, badStatus); });
    rejectsRuntime([&] { ufmtval_getString_nid_postfix(nullptr, nullptr, roZero); });
    Require(ufmtval_getString_nid_postfix(dummyValue, nullptr, roFailing) == nullptr);

    rejectsRuntime([&] { ufmtval_nextPosition_nid_postfix(nullptr, nullptr, badStatus); });
    rejectsRuntime([&] { ufmtval_nextPosition_nid_postfix(nullptr, nullptr, roZero); });
    rejectsRuntime([&] { ufmtval_nextPosition_nid_postfix(dummyValue, nullptr, roZero); });
    Require(ufmtval_nextPosition_nid_postfix(dummyValue, nullptr, roFailing) == 0);

#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
}

}

void CheckIcuFormattedValue() {
    UfmtvalGetString::Run();
    UfmtvalNextPosition::Run();
    FormattedValueGuestPointers::Run();
}
