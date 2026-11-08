#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udtitvfmt_formatToResult_nid_postfix),
                             void (APS5_VABI*)(const UDateIntervalFormat*, double, double, UFormattedDateInterval*, std::int32_t*)>);

namespace {

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t InvalidFormatError = 3;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t BufferOverflowError = 15;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t WarningStart = -128;
constexpr std::int32_t NotTerminatedWarning = -124;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date interval formatToResult check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

void PreexistingFailureShortCircuits() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    auto* unalignedResult = reinterpret_cast<UFormattedDateInterval*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t incoming : {IllegalArgumentError, InvalidFormatError, MemoryAllocationError, BufferOverflowError, UnsupportedError, 27, 65536}) {
        std::int32_t status = incoming;
        udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, -1000.5, 2000.25, unalignedResult, &status);
        Require(status == incoming);
    }
}

void NullResultValidation() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t incoming : {ZeroError, WarningStart, NotTerminatedWarning, -1}) {
        std::int32_t status = incoming;
        udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, &status);
        Require(status == IllegalArgumentError);

        status = incoming;
        udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, std::numeric_limits<double>::quiet_NaN(),
                                             std::numeric_limits<double>::infinity(), nullptr, &status);
        Require(status == IllegalArgumentError);
    }
}

void ForeignResultThrowsWithoutTouchingIt() {
    const auto* unalignedFormatter = reinterpret_cast<const UDateIntervalFormat*>(static_cast<std::uintptr_t>(1));
    for (std::int32_t magicLike : {0, 1, -1, 0x55434600, 0x46444956, 0x56494446}) {
        alignas(8) std::array<std::uint8_t, 32> raw{};
        raw.fill(0xa5);
        std::memcpy(raw.data(), &magicLike, sizeof(magicLike));
        const auto snapshot = raw;
        auto* forged = reinterpret_cast<UFormattedDateInterval*>(raw.data());

        for (std::int32_t incoming : {ZeroError, WarningStart, NotTerminatedWarning, -1}) {
            std::int32_t status = incoming;
            RequireThrows([&] { udtitvfmt_formatToResult_nid_postfix(nullptr, 10.0, 20.0, forged, &status); });
            Require(status == incoming);
            Require(raw == snapshot);

            status = incoming;
            RequireThrows([&] {
                udtitvfmt_formatToResult_nid_postfix(unalignedFormatter, -50.0, 50.0, forged, &status);
            });
            Require(status == incoming);
            Require(raw == snapshot);
        }
    }
}

void UnsupportedPathsThrow() {
    RequireThrows([] { udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 0.0, nullptr, nullptr); });

    alignas(8) std::array<std::uint8_t, 32> raw{};
    auto* nonNullResult = reinterpret_cast<UFormattedDateInterval*>(raw.data());
    for (std::int32_t incoming : {ZeroError, NotTerminatedWarning, -1}) {
        std::int32_t status = incoming;
        RequireThrows([&] { udtitvfmt_formatToResult_nid_postfix(nullptr, 0.0, 1000.0, nonNullResult, &status); });
        Require(status == incoming);
    }
}

}

int main() {
    PreexistingFailureShortCircuits();
    NullResultValidation();
    ForeignResultThrowsWithoutTouchingIt();
    UnsupportedPathsThrow();
    return 0;
}
