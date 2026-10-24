#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceAudioOutExGetMonitorInfo(int, int, void*, std::uint32_t);
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

namespace {

constexpr std::uint32_t titleSize = 0x400;
constexpr std::size_t descriptorCountOffset = 0x98;
constexpr std::size_t guardSize = 16;
constexpr std::uint8_t untouched = 0xA5;
constexpr int invalidPointer = static_cast<int>(0x80260004);

void CheckEmpty(int index) {
    constexpr auto size = titleSize;
    std::array<std::uint8_t, titleSize + guardSize> buffer{};
    buffer.fill(untouched);
    Require(sceAudioOutExGetMonitorInfo(0, index, buffer.data(), size) == 0, "the monitor query must succeed");
    for (std::uint32_t i = 0; i < size; i++) {
        Require(buffer[i] == 0, "the monitor info must describe a sink without audio descriptors");
    }
    std::uint32_t descriptors = 0xFFFFFFFFu;
    std::memcpy(&descriptors, buffer.data() + descriptorCountOffset, sizeof(descriptors));
    Require(descriptors == 0, "the audio descriptor count must be 0");
    for (std::size_t i = size; i < buffer.size(); i++) {
        Require(buffer[i] == untouched, "nothing past the declared size may be written");
    }
}

}

int main() {
    for (const int index : {0, 1, -1, 100}) CheckEmpty(index);
    std::array<std::uint8_t, titleSize + guardSize> buffer;
    buffer.fill(untouched);
    for (const std::uint32_t size : {0u, 0xA0u, titleSize - 1, titleSize + 1, 0xffffffffu}) {
        Require(sceAudioOutExGetMonitorInfo(0, 0, buffer.data(), size) == static_cast<int>(0x80260006), "exact size required");
        Require(sceAudioOutExGetMonitorInfo(0, 0, nullptr, size) == invalidPointer, "pointer checked before size");
        Require(sceAudioOutExGetMonitorInfo(1, 0, nullptr, size) == static_cast<int>(0x80260003), "type checked before pointer");
        for (const auto value : buffer) Require(value == untouched, "failure preserves output");
    }
    return 0;
}
