#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&unum_clone_nid_postfix), UNumberFormat* (APS5_VABI*)(const UNumberFormat*, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU number format clone check failed at line %u\n", location.line());
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

    std::puts("ICU number format clone tests passed");
}
