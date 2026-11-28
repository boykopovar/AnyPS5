#pragma once

#include <cstdint>

struct GuestUuid {
    std::uint32_t time_low;
    std::uint16_t time_mid;
    std::uint16_t time_hi_and_version;
    std::uint8_t clock_seq_hi_and_reserved;
    std::uint8_t clock_seq_low;
    std::uint8_t node[6];
};
