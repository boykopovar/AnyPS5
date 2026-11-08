#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UnumClone {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&unum_clone_nid_postfix), UNumberFormat* (APS5_VABI*)(const UNumberFormat*, std::int32_t*)>);

namespace {

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
    auto formatStorage = Filler();
    const auto* fmt = reinterpret_cast<const UNumberFormat*>(formatStorage.data());

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unum_clone_nid_postfix(nullptr, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(unum_clone_nid_postfix(fmt, &status) == nullptr);
        Require(status == incoming);
        Require(Unchanged(formatStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unum_clone_nid_postfix(nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { unum_clone_nid_postfix(fmt, &status); });
        Require(status == incoming);
        Require(Unchanged(formatStorage));
    }

    RequireThrows([&] { unum_clone_nid_postfix(nullptr, nullptr); });
}

} // namespace UnumClone

namespace UnumClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&unum_close_nid_postfix), void (APS5_VABI*)(UNumberFormat*)>);

namespace {

template<typename TAction>
bool ThrowsRuntimeError(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

void Run() {
    unum_close_nid_postfix(nullptr);
    UNumberFormat* nullFormat = nullptr;
    unum_close_nid_postfix(nullFormat);

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0x5a);
    auto* nonNullFormat = reinterpret_cast<UNumberFormat*>(storage.data());
    Require(ThrowsRuntimeError([&] { unum_close_nid_postfix(nonNullFormat); }), "non-null UNumberFormat must throw");
    for (const std::uint8_t byte : storage) {
        Require(byte == 0x5a, "non-null UNumberFormat must not be modified before throwing");
    }

    auto* unmappedFormat = reinterpret_cast<UNumberFormat*>(std::uintptr_t{1});
    Require(ThrowsRuntimeError([&] { unum_close_nid_postfix(unmappedFormat); }), "unmapped non-null UNumberFormat must throw without dereference");
}

} // namespace UnumClose

namespace UnumFormatDecimal {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&unum_formatDecimal_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, const char*, std::int32_t, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);

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
    auto formatStorage = Filler();
    const auto* fmt = reinterpret_cast<const UNumberFormat*>(formatStorage.data());
    std::array<char16_t, 4> result{u'x', u'y', u'z', 0};
    const auto probeResult = result;

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unum_formatDecimal_nid_postfix(nullptr, nullptr, 0, nullptr, 0, nullptr, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(unum_formatDecimal_nid_postfix(fmt, "123", -1, result.data(), 4, nullptr, &status) == -1);
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(unum_formatDecimal_nid_postfix(fmt, "123", 3, nullptr, 4, nullptr, &status) == -1);
        Require(status == IllegalArgument);

        status = incoming;
        Require(unum_formatDecimal_nid_postfix(fmt, "123", 3, result.data(), -1, nullptr, &status) == -1);
        Require(status == IllegalArgument);
        Require(result == probeResult);
        Require(Unchanged(formatStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unum_formatDecimal_nid_postfix(fmt, "123", -1, result.data(), 4, nullptr, &status); });
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatStorage));

        status = incoming;
        RequireThrows([&] { unum_formatDecimal_nid_postfix(fmt, nullptr, 0, nullptr, 0, nullptr, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { unum_formatDecimal_nid_postfix(fmt, "123", 3, result.data(), 4, nullptr, nullptr); });
    Require(result == probeResult);
}

} // namespace UnumFormatDecimal

namespace UnumFormatDouble {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unum_formatDouble_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, double, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto destination = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));
    const auto position = reinterpret_cast<UFieldPosition*>(const_cast<std::uint32_t*>(storage.data()));
    const auto format = reinterpret_cast<UNumberFormat*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(unum_formatDouble_nid_postfix(format, 12.5, destination, 8, position, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(unum_formatDouble_nid_postfix(format, -1.0, nullptr, 0, nullptr, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(unum_formatDouble_nid_postfix(nullptr, 0.0, destination, -1, position, &status) == -1);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { unum_formatDouble_nid_postfix(format, 12.5, destination, 8, position, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { unum_formatDouble_nid_postfix(format, 12.5, nullptr, 0, nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { unum_formatDouble_nid_postfix(format, 12.5, destination, -1, position, &status); });
        Require(status == incoming);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { unum_formatDouble_nid_postfix(format, 0.0, destination, 8, position, nullptr); });
}

} // namespace UnumFormatDouble

namespace UnumGetSymbol {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unum_getSymbol_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const std::array<std::int32_t, 2> usable{0, 27};
    const std::array<std::int32_t, 4> beyond{-1, 28, 29, 0x7fffffff};
    const auto storage = Filler();
    const auto buffer = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));
    const auto format = reinterpret_cast<UNumberFormat*>(const_cast<std::uint32_t*>(storage.data()));
    auto nullStatusReturns = std::array<std::int32_t, 3>{-1, -1, -1};

    RequireNoThrow([&] { nullStatusReturns[0] = unum_getSymbol_nid_postfix(format, 0, buffer, 8, nullptr); });
    RequireNoThrow([&] { nullStatusReturns[1] = unum_getSymbol_nid_postfix(nullptr, 0, nullptr, 0, nullptr); });
    RequireNoThrow([&] { nullStatusReturns[2] = unum_getSymbol_nid_postfix(format, 99, buffer, 8, nullptr); });
    Require(nullStatusReturns == std::array<std::int32_t, 3>{0, 0, 0});

    for (const auto incoming : failing) {
        for (const auto symbol : usable) {
            std::int32_t status = incoming;
            Require(unum_getSymbol_nid_postfix(format, symbol, buffer, 8, &status) == 0);
            Require(status == incoming);
        }

        for (const auto symbol : beyond) {
            std::int32_t status = incoming;
            Require(unum_getSymbol_nid_postfix(nullptr, symbol, buffer, 8, &status) == 0);
            Require(status == incoming);
        }
    }

    for (const auto incoming : passing) {
        for (const auto symbol : usable) {
            std::int32_t status = incoming;
            RequireThrows([&] { unum_getSymbol_nid_postfix(format, symbol, buffer, 8, &status); });
            Require(status == incoming);
        }

        for (const auto symbol : beyond) {
            std::int32_t status = incoming;
            Require(unum_getSymbol_nid_postfix(format, symbol, buffer, 8, &status) == 0);
            Require(status == IllegalArgument);

            status = incoming;
            Require(unum_getSymbol_nid_postfix(nullptr, symbol, nullptr, 0, &status) == 0);
            Require(status == IllegalArgument);
        }
    }

    Require(Unchanged(storage));
}

} // namespace UnumGetSymbol

namespace UnumOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&unum_open_nid_postfix), UNumberFormat* (APS5_VABI*)(std::int32_t, const char16_t*, std::int32_t, const char*, UParseError*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const std::array<std::int32_t, 4> inRange{0, 5, 9, 16};
    const std::array<std::int32_t, 5> outOfRange{-1, -0x7fffffff, 17, 18, 0x7fffffff};
    const auto storage = Filler();
    const auto pattern = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));
    const auto parseError = reinterpret_cast<UParseError*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        for (const auto style : inRange) {
            std::int32_t status = incoming;
            Require(unum_open_nid_postfix(style, pattern, 4, "en", parseError, &status) == nullptr);
            Require(status == incoming);
        }

        for (const auto style : outOfRange) {
            std::int32_t status = incoming;
            Require(unum_open_nid_postfix(style, nullptr, -1, nullptr, nullptr, &status) == nullptr);
            Require(status == incoming);
        }
    }

    for (const auto incoming : passing) {
        for (const auto style : inRange) {
            std::int32_t status = incoming;
            RequireThrows([&] { unum_open_nid_postfix(style, pattern, 4, "en", parseError, &status); });
            Require(status == incoming);
        }

        for (const auto style : outOfRange) {
            std::int32_t status = incoming;
            Require(unum_open_nid_postfix(style, pattern, 4, "en", parseError, &status) == nullptr);
            Require(status == UnsupportedError);
        }
    }

    Require(Unchanged(storage));
    RequireThrows([&] { unum_open_nid_postfix(0, pattern, 4, "en", parseError, nullptr); });
}

} // namespace UnumOpen

void CheckIcuNumberFormat() {
    UnumClone::Run();
    UnumClose::Run();
    UnumFormatDecimal::Run();
    UnumFormatDouble::Run();
    UnumGetSymbol::Run();
    UnumOpen::Run();
}
