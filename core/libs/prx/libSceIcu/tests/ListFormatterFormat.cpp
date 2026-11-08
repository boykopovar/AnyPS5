#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ulistfmt_format_nid_postfix), std::int32_t (APS5_VABI*)(const UListFormatter*, const char16_t* const*, const std::int32_t*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU list formatter buffer formatting check failed at line %u\n", location.line());
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
    auto formatterStorage = Filler();
    const auto* formatter = reinterpret_cast<const UListFormatter*>(formatterStorage.data());
    std::array<char16_t, 4> result{u'x', u'y', u'z', 0};
    const auto probeResult = result;
    std::array<std::int32_t, 1> lengths{3};
    const char16_t* strings[1] = {result.data()};

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ulistfmt_format_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, 0, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, &status) == -1);
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, nullptr, 4, &status) == -1);
        Require(status == IllegalArgument);

        status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), -1, &status) == -1);
        Require(status == IllegalArgument);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, &status); });
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));

        status = incoming;
        RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, nullptr, nullptr, 0, nullptr, 0, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, nullptr); });
    Require(result == probeResult);

    std::puts("ICU list formatter buffer formatting tests passed");
}
