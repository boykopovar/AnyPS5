#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ulistfmt_openForType_nid_postfix), UListFormatter* (APS5_VABI*)(const char*, UListFormatterType, UListFormatterWidth, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU list formatter creation by type check failed at line %u\n", location.line());
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

} // namespace

int main() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ulistfmt_openForType_nid_postfix("en", 0, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ulistfmt_openForType_nid_postfix(nullptr, 3, 2, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_openForType_nid_postfix("en", 0, 0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ulistfmt_openForType_nid_postfix(nullptr, 3, 2, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { ulistfmt_openForType_nid_postfix("en", 0, 0, nullptr); });

    std::puts("ICU list formatter creation by type tests passed");
}
