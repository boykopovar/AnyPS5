#pragma once

#include <cstdint>

struct alignas(4) UnwindFixtureHeader {
    std::uint8_t version = 1;
    std::uint8_t framePointerEncoding = 0x1b;
    std::uint8_t countEncoding = 0x03;
    std::uint8_t tableEncoding = 0x3b;
    std::int32_t frames = 0;
    std::uint32_t count = 0;
};

struct alignas(4) UnwindFixtureFrames {
    std::uint32_t cieLength = 12;
    std::uint32_t cieId = 0;
    std::uint8_t cie[8] = {1, 0, 1, 0x78, 16, 0, 0, 0};
    std::uint32_t terminator = 0;
};

struct UnwindFixture {
    UnwindFixtureHeader header;
    UnwindFixtureFrames frames;
};

extern "C" UnwindFixture* GetUnwindFixture();
