#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ulistfmt_close_nid_postfix), void (APS5_VABI*)(UListFormatter*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU list formatter: %s\n", message);
    std::abort();
}

bool ThrowsNotImplemented(UListFormatter* listfmt) {
    try {
        ulistfmt_close_nid_postfix(listfmt);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    ulistfmt_close_nid_postfix(nullptr);
    ulistfmt_close_nid_postfix(nullptr);

    alignas(8) std::array<std::uint8_t, 64> bytes{};
    bytes.fill(0xa5);
    const auto expected = bytes;
    Require(ThrowsNotImplemented(reinterpret_cast<UListFormatter*>(bytes.data())), "non-null list formatter throws");
    Require(bytes == expected, "non-null list formatter bytes stay untouched");
    Require(ThrowsNotImplemented(reinterpret_cast<UListFormatter*>(std::uintptr_t{1})), "unmapped non-null list formatter throws before dereference");

    std::puts("ICU list formatter tests passed");
}
