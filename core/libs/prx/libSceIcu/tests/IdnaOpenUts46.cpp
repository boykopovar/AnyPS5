#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uidna_openUTS46_nid_postfix), UIDNA* (APS5_VABI*)(std::uint32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU IDNA UTS46 opening check failed at line %u\n", location.line());
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
        Require(uidna_openUTS46_nid_postfix(0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(uidna_openUTS46_nid_postfix(0xffffffffu, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { uidna_openUTS46_nid_postfix(0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { uidna_openUTS46_nid_postfix(0xffffffffu, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { uidna_openUTS46_nid_postfix(0, nullptr); });

    std::puts("ICU IDNA UTS46 opening tests passed");
}
