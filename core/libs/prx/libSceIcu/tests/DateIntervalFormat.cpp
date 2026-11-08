#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udtitvfmt_close_nid_postfix), void (APS5_VABI*)(UDateIntervalFormat*)>);

namespace {

void CheckNonNullThrows() {
    alignas(8) unsigned char dummy[8]{};
    try {
        udtitvfmt_close_nid_postfix(reinterpret_cast<UDateIntervalFormat*>(dummy));
    } catch (const std::exception&) {
        for (const auto byte : dummy) {
            if (byte != 0) std::abort();
        }
        return;
    }
    std::fputs("non-null UDateIntervalFormat did not throw\n", stderr);
    std::abort();
}

} // namespace

int main() {
    udtitvfmt_close_nid_postfix(nullptr);
    udtitvfmt_close_nid_postfix(nullptr);
    CheckNonNullThrows();
    std::puts("ICU date interval format tests passed");
}
