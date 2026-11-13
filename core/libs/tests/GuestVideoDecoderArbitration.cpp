#include "prx/libSceVideoDecoderArbitration/libSceVideoDecoderArbitration.hpp"

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
    std::uint64_t count;
};

} // namespace

int main() {
    constexpr int invalidArgument = static_cast<int>(0x81570001u);
    constexpr int outOfRange = static_cast<int>(0x81570004u);
    int callbackTarget = 0;
    for (int call = 0; call < 2; ++call) {
        Require(sceVideoDecoderArbitrationEnableSuspendMode() == 0);
        Require(sceVideoDecoderArbitrationAcceptEvent(2) == invalidArgument);
        Require(sceVideoDecoderArbitrationAcceptEvent(0xFFFFFFFFu) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(1, &callbackTarget) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(0, nullptr) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(0x100000000u, nullptr) == invalidArgument);
        Require(sceVideoDecoderArbitrationInitialize(nullptr) == invalidArgument);
        for (std::uint64_t size : std::array<std::uint64_t, 4>{std::uint64_t{0}, std::uint64_t{0x10}, std::uint64_t{0x19}, std::uint64_t{0x100000018}}) {
            ArbitrationParam param{size, 0x200, 1};
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
        }
        for (std::uint32_t value : std::array<std::uint32_t, 4>{0u, 0xFFu, 0x300u, 0xFFFFFFFFu}) {
            ArbitrationParam param{0x18, value, 1};
            Require(sceVideoDecoderArbitrationInitialize(&param) == outOfRange);
        }
        for (std::uint64_t count : std::array<std::uint64_t, 3>{std::uint64_t{0}, std::uint64_t{0x80}, std::uint64_t{0xFFFFFFFFFFFFFFFF}}) {
            ArbitrationParam param{0x18, 0x100, count};
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
            param.value = 0x2FF;
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
        }
    }
    return 0;
}
