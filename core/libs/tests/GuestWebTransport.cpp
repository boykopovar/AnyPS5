#include "prx/libSceWebTransport/libSceWebTransport.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int invalidModeError = static_cast<int>(0x80100004);
constexpr int nullStatsError = static_cast<int>(0x80100005);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void DummyCallback() {}

}

int main() {
    sceWebTransportUnknown00(0x3u);
    sceWebTransportUnknown01(reinterpret_cast<void*>(&DummyCallback));
    sceWebTransportUnknown01(nullptr);

    const std::uint32_t pair[2] = {1024u, 2048u};
    Require(sceWebTransportUnknown03(pair) == 0);
    Require(sceWebTransportUnknown03(nullptr) == 0);

    Require(sceWebTransportUnknown04(nullptr) == nullStatsError);
    std::array<std::uint32_t, 12> stats{};
    stats.fill(0xdeadbeefu);
    Require(sceWebTransportUnknown04(stats.data()) == 0);
    for (const std::uint32_t v : stats) {
        Require(v == 0u);
    }

    std::array<std::uint64_t, 10> counters{};
    counters.fill(0xfeedfacecafebeefULL);
    sceWebTransportUnknown05(counters.data());
    for (const std::uint64_t c : counters) {
        Require(c == 0ULL);
    }

    Require(sceWebTransportUnknown06() == 0);

    sceWebTransportUnknown14(nullptr);
    std::array<std::uint8_t, 0xf8> config{};
    config.fill(0xaa);
    sceWebTransportUnknown14(config.data());
    for (std::size_t i = 0; i < 0x50; ++i) {
        Require(config[i] == 0);
    }
    std::uint64_t val = 0;
    std::memcpy(&val, config.data() + 0x50, sizeof(val));
    Require(val == 0x403e000000000000ULL);
    std::memcpy(&val, config.data() + 0x58, sizeof(val));
    Require(val == 0x403e000000000000ULL);
    std::memcpy(&val, config.data() + 0x60, sizeof(val));
    Require(val == 0x4024000000000000ULL);
    std::memcpy(&val, config.data() + 0x68, sizeof(val));
    Require(val == 0x3fd54fdf3b645a1dULL);
    std::memcpy(&val, config.data() + 0x70, sizeof(val));
    Require(val == 0x3f9999999999999aULL);
    std::memcpy(&val, config.data() + 0x78, sizeof(val));
    Require(val == 3ULL);
    std::memcpy(&val, config.data() + 0x80, sizeof(val));
    Require(val == 2ULL);
    std::memcpy(&val, config.data() + 0x88, sizeof(val));
    Require(val == 100ULL);
    std::memcpy(&val, config.data() + 0x90, sizeof(val));
    Require(val == 103ULL);
    for (std::size_t i = 0x98; i < 0xa8; ++i) {
        Require(config[i] == 0);
    }
    for (std::size_t off = 0xa8; off < 0xc8; off += 8) {
        std::memcpy(&val, config.data() + off, sizeof(val));
        Require(val == 0x100000ULL);
    }
    for (std::size_t i = 0xc8; i < 0xf0; ++i) {
        Require(config[i] == 0);
    }
    for (std::size_t i = 0xf0; i < config.size(); ++i) {
        Require(config[i] == 0xaa);
    }

    Require(sceWebTransportUnknown15(0u) == 0);
    Require(sceWebTransportUnknown15(1u) == 0);
    Require(sceWebTransportUnknown15(2u) == invalidModeError);
    Require(sceWebTransportUnknown15(0xffffffffu) == invalidModeError);
    return 0;
}
