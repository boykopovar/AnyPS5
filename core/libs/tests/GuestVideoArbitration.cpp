#include "prx/libSceVideoArbitration/libSceVideoArbitration.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = EACCES;
}

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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    constexpr int notInitialized = static_cast<int>(0x819E0001u);
    constexpr int invalidArgument = static_cast<int>(0x819E0002u);
    constexpr int alreadyInitialized = static_cast<int>(0x819E0003u);
    int callbackTarget = 0;
    int otherCallbackTarget = 0;
    for (int call = 0; call < 2; ++call) {
        Require(sceVideoArbitrationEnable(0, &callbackTarget) == notInitialized);
        Require(sceVideoArbitrationEnable(1, nullptr) == notInitialized);
        Require(sceVideoArbitrationAcceptEvent(0) == notInitialized);
        Require(sceVideoArbitrationAcceptEvent(1) == notInitialized);
        Require(sceVideoArbitrationInitialize(nullptr) == invalidArgument);
        errno = EAGAIN;
        Require(sceVideoArbitrationInitialize(reinterpret_cast<const void*>(std::uintptr_t{1})) == invalidArgument);
        Require(errno == EAGAIN);
        for (std::uint64_t size : std::array<std::uint64_t, 5>{std::uint64_t{0}, std::uint64_t{0x10}, std::uint64_t{0x17}, std::uint64_t{0x19},
                                   std::uint64_t{0x100000018}}) {
            ArbitrationParam param{size, 0x200, 1};
            errno = EAGAIN;
            Require(sceVideoArbitrationInitialize(&param) == invalidArgument);
            Require(errno == EAGAIN);
        }
    }
    ArbitrationParam validParam{0x18, 0x200, 1};
    errno = EAGAIN;
    Require(sceVideoArbitrationInitialize(&validParam) == 0);
    Require(errno == EAGAIN);
    Require(sceVideoArbitrationInitialize(nullptr) == invalidArgument);
    ArbitrationParam badSizeParam{0x10, 0x200, 1};
    errno = EAGAIN;
    Require(sceVideoArbitrationInitialize(&badSizeParam) == invalidArgument);
    Require(errno == EAGAIN);
    Require(sceVideoArbitrationInitialize(&validParam) == alreadyInitialized);
    Require(sceVideoArbitrationEnable(1, &callbackTarget) == invalidArgument);
    Require(sceVideoArbitrationEnable(std::uint64_t{0x100000000ull}, &callbackTarget) == invalidArgument);
    Require(sceVideoArbitrationEnable(0, nullptr) == invalidArgument);
    Require(sceVideoArbitrationEnable(1, nullptr) == invalidArgument);
    Require(sceVideoArbitrationEnable(0, &callbackTarget) == 0);
    Require(sceVideoArbitrationEnable(0, &otherCallbackTarget) == 0);
    Require(sceVideoArbitrationAcceptEvent(0) == alreadyInitialized);
    Require(sceVideoArbitrationAcceptEvent(1) == alreadyInitialized);
    Require(sceVideoArbitrationAcceptEvent(0xFFFFFFFFu) == alreadyInitialized);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    return 0;
}
