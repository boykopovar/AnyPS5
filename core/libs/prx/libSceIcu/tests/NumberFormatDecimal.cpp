#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&unum_formatDecimal_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, const char*, std::int32_t, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU decimal string formatting check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

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

int main() {
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

    std::puts("ICU decimal string formatting tests passed");
}
