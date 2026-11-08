#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ureldatefmt_close_nid_postfix), void (APS5_VABI*)(URelativeDateTimeFormatter*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU relative date-time formatter: %s\n", message);
    std::abort();
}

}

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    ureldatefmt_close_nid_postfix(nullptr);
    ureldatefmt_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "null disposal does not hit unimplemented stub");

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    auto* foreign = reinterpret_cast<URelativeDateTimeFormatter*>(storage.data());
    bool threw = false;
    try {
        ureldatefmt_close_nid_postfix(foreign);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && NotImplementedCalls_nid_no_patch() == before + 1, "non-null handle throws NotImplemented_nid_no_patch");
    for (const auto byte : storage) {
        Require(byte == 0xa5, "non-null handle memory is untouched");
    }
    return 0;
}
