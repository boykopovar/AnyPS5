#include "prx/libSceVideoArbitration/libSceVideoArbitration.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

struct ArbitrationParam {
    std::uint64_t size;
    std::uint32_t value;
    std::uint64_t extra;
};

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x819E0001u);
    constexpr int invalidArgument = static_cast<int>(0x819E0002u);
    for (int call = 0; call < 2; ++call) {
        Require(sceVideoArbitrationEnable() == notInitialized);
        Require(sceVideoArbitrationAcceptEvent() == notInitialized);
        Require(sceVideoArbitrationInitialize(nullptr) == invalidArgument);
        for (std::uint64_t size : std::array<std::uint64_t, 5>{std::uint64_t{0}, std::uint64_t{0x10}, std::uint64_t{0x17}, std::uint64_t{0x19},
                                   std::uint64_t{0x100000018}}) {
            ArbitrationParam param{size, 0x200, 1};
            Require(sceVideoArbitrationInitialize(&param) == invalidArgument);
        }
    }
    return 0;
}
