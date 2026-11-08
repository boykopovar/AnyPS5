#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ufmtval_getString_nid_postfix), const char16_t* (APS5_VABI*)(const UFormattedValue*, std::int32_t*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU formatted value get-string check failed at line %u\n", location.line());
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

    RequireThrows([&] { ufmtval_getString_nid_postfix(nullptr, nullptr, nullptr); });

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        std::int32_t length = 0x5a5a5a5a;
        RequireThrows([&] { ufmtval_getString_nid_postfix(value, &length, &status); });
        Require(status == incoming);
        Require(length == 0x5a5a5a5a);
        Require(Unchanged(storage));
    }

    std::puts("ICU formatted value get-string tests passed");
}
