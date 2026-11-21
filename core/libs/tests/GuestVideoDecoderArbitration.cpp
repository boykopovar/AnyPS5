#include "prx/libSceVideoDecoderArbitration/libSceVideoDecoderArbitration.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

struct ArbitrationParam {
    std::uint64_t size;
    std::uint32_t value;
    std::uint64_t count;
};

} // namespace

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    constexpr int invalidArgument = static_cast<int>(0x81570001u);
    constexpr int alreadyInitialized = static_cast<int>(0x81570002u);
    constexpr int outOfRange = static_cast<int>(0x81570004u);
    int callbackTarget = 0;
    int otherCallbackTarget = 0;
    for (int call = 0; call < 2; ++call) {
        Require(sceVideoDecoderArbitrationEnableSuspendMode() == 0);
        Require(sceVideoDecoderArbitrationAcceptEvent(2) == invalidArgument);
        Require(sceVideoDecoderArbitrationAcceptEvent(0xFFFFFFFFu) == invalidArgument);
        Require(sceVideoDecoderArbitrationAcceptEvent(0) == invalidArgument);
        Require(sceVideoDecoderArbitrationAcceptEvent(1) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(1, &callbackTarget) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(0, nullptr) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(0x100000000u, nullptr) == invalidArgument);
        Require(sceVideoDecoderArbitrationEnable(0, &callbackTarget) == invalidArgument);
        Require(sceVideoDecoderArbitrationInitialize(nullptr) == invalidArgument);
        errno = 71;
        Require(sceVideoDecoderArbitrationInitialize(reinterpret_cast<const void*>(std::uintptr_t{1})) == invalidArgument);
        Require(errno == 71);
        for (std::uint64_t size : std::array<std::uint64_t, 4>{std::uint64_t{0}, std::uint64_t{0x10}, std::uint64_t{0x19}, std::uint64_t{0x100000018}}) {
            ArbitrationParam param{size, 0x200, 1};
            errno = 71;
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
            Require(errno == 71);
        }
        for (std::uint32_t value : std::array<std::uint32_t, 4>{0u, 0xFFu, 0x300u, 0xFFFFFFFFu}) {
            ArbitrationParam param{0x18, value, 1};
            errno = 71;
            Require(sceVideoDecoderArbitrationInitialize(&param) == outOfRange);
            Require(errno == 71);
        }
        for (std::uint64_t count : std::array<std::uint64_t, 3>{std::uint64_t{0}, std::uint64_t{0x80}, std::uint64_t{0xFFFFFFFFFFFFFFFF}}) {
            ArbitrationParam param{0x18, 0x100, count};
            errno = 71;
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
            Require(errno == 71);
            param.value = 0x2FF;
            Require(sceVideoDecoderArbitrationInitialize(&param) == invalidArgument);
        }
    }
    ArbitrationParam validParam{0x18, 0x200, 1};
    errno = 71;
    Require(sceVideoDecoderArbitrationInitialize(&validParam) == 0);
    Require(errno == 71);
    Require(sceVideoDecoderArbitrationInitialize(nullptr) == invalidArgument);
    ArbitrationParam badSizeParam{0x10, 0x200, 1};
    errno = 71;
    Require(sceVideoDecoderArbitrationInitialize(&badSizeParam) == invalidArgument);
    Require(errno == 71);
    ArbitrationParam badValueParam{0x18, 0x50, 1};
    errno = 71;
    Require(sceVideoDecoderArbitrationInitialize(&badValueParam) == alreadyInitialized);
    Require(errno == 71);
    errno = 71;
    Require(sceVideoDecoderArbitrationInitialize(&validParam) == alreadyInitialized);
    Require(errno == 71);
    Require(sceVideoDecoderArbitrationEnable(1, &callbackTarget) == invalidArgument);
    Require(sceVideoDecoderArbitrationEnable(0, nullptr) == invalidArgument);
    Require(sceVideoDecoderArbitrationEnable(0, &callbackTarget) == 0);
    Require(sceVideoDecoderArbitrationEnable(0, &callbackTarget) == alreadyInitialized);
    Require(sceVideoDecoderArbitrationEnable(0, &otherCallbackTarget) == alreadyInitialized);
    Require(sceVideoDecoderArbitrationAcceptEvent(2) == invalidArgument);
    Require(sceVideoDecoderArbitrationAcceptEvent(0) == alreadyInitialized);
    Require(sceVideoDecoderArbitrationAcceptEvent(1) == alreadyInitialized);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
