#include "prx/libkernel/Pthread/include/MeasuredStack.hpp"

#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

static bool Inside(std::uintptr_t value, std::uintptr_t low, std::uintptr_t high) {
    return value >= low && value < high;
}

int main() {
    constexpr std::uintptr_t hostHigh = 0x2000000;
    constexpr std::size_t guest = 0x200000;
    constexpr std::size_t hostMargin = std::size_t{1} << 20;
    constexpr std::size_t shallowProbe = 0x1000;
    constexpr std::uintptr_t call = hostHigh - 0x100;
    constexpr std::uintptr_t local = call - 32;
    const std::uintptr_t earlyLow = hostHigh - shallowProbe - guest;
    const std::uintptr_t earlyHigh = hostHigh - shallowProbe;
    Require(!Inside(local, earlyLow, earlyHigh));

    constexpr std::size_t depth = hostHigh - call;
    constexpr std::uintptr_t hostLow = hostHigh - (guest + hostMargin + depth);
    const MeasuredGuestStack window = MeasureGuestStackWindow(hostLow, hostHigh, call, guest, hostMargin);
    Require(Inside(local, window.low, window.high));
    Require(window.high - window.low == guest);
    Require(window.low >= hostLow && window.high <= hostHigh);
    Require(window.low >= hostLow + hostMargin);

    constexpr std::size_t modestDepth = 64u * 1024u;
    constexpr std::uintptr_t modestCall = hostHigh - modestDepth;
    constexpr std::uintptr_t modestHostLow = hostHigh - (guest + hostMargin + modestDepth);
    const MeasuredGuestStack modest = MeasureGuestStackWindow(modestHostLow, hostHigh, modestCall, guest, hostMargin);
    Require(Inside(modestCall - 32, modest.low, modest.high));
    Require(modest.low >= modestHostLow + hostMargin);

    constexpr std::size_t deep = hostMargin + 0x2000;
    constexpr std::uintptr_t deepCall = hostHigh - deep;
    constexpr std::uintptr_t shortHostLow = hostHigh - (guest + hostMargin + shallowProbe);
    const MeasuredGuestStack overrun = MeasureGuestStackWindow(shortHostLow, hostHigh, deepCall, guest, hostMargin);
    Require(overrun.low < shortHostLow + hostMargin || overrun.high == 0);
    return 0;
}
