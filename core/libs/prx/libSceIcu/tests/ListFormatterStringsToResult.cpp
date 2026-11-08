#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ulistfmt_formatStringsToResult_nid_postfix), void (APS5_VABI*)(const UListFormatter*, const char16_t* const*, const std::int32_t*, std::int32_t, UFormattedList*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU list formatter result population check failed at line %u\n", location.line());
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
    auto resultStorage = Filler();
    const auto* formatter = reinterpret_cast<const UListFormatter*>(formatterStorage.data());
    auto* result = reinterpret_cast<UFormattedList*>(resultStorage.data());
    std::array<char16_t, 4> item{u'a', u'b', u'c', 0};
    const auto probeItem = item;
    std::array<std::int32_t, 1> lengths{3};
    const char16_t* strings[1] = {item.data()};

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, &status);
        Require(status == incoming);
        Require(Unchanged(formatterStorage) && Unchanged(resultStorage));
        Require(item == probeItem);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, nullptr, &status);
        Require(status == IllegalArgument);
        Require(item == probeItem);

        status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, &status);
        Require(status == IllegalArgument);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, &status); });
        Require(status == incoming);
        Require(Unchanged(formatterStorage) && Unchanged(resultStorage));
        Require(item == probeItem);
    }

    RequireThrows([&] { ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, nullptr); });
    Require(Unchanged(resultStorage));

    std::puts("ICU list formatter result population tests passed");
}
