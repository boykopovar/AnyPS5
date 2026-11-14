#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_INCLUDE_MEASUREDSTACK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_INCLUDE_MEASUREDSTACK_HPP

#include <cstddef>
#include <cstdint>

struct MeasuredGuestStack {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
};

inline MeasuredGuestStack MeasureGuestStackWindow(std::uintptr_t hostLow, std::uintptr_t hostHigh, std::uintptr_t callPointer, std::size_t guestBytes, std::size_t hostReserve) {
    MeasuredGuestStack window;
    if (hostHigh <= hostLow || guestBytes == 0 || callPointer <= hostLow || callPointer > hostHigh)
        return window;
    constexpr std::uintptr_t alignment = 16;
    const auto high = callPointer & ~(alignment - 1);
    if (high < guestBytes || high <= hostLow || high > hostHigh)
        return window;
    const auto low = high - static_cast<std::uintptr_t>(guestBytes);
    if (low < hostLow + hostReserve)
        return window;
    window.low = low;
    window.high = high;
    return window;
}

#endif
