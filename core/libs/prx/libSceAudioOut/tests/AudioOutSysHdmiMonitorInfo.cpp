#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceAudioOutSysGetHdmiMonitorInfo(int, void*, std::uint32_t);
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

namespace {

constexpr int monitorTypeHdmi = 1;
constexpr std::uint32_t infoSize = 0x180;
constexpr std::size_t descriptorCountOffset = 0x94;
constexpr std::size_t guardSize = 16;
constexpr std::uint8_t untouched = 0xA5;
constexpr int invalidPointer = static_cast<int>(0x80260004);

bool Throws(int type, std::uint32_t size, const std::string& expected) {
    std::array<std::uint8_t, infoSize> info{};
    try {
        sceAudioOutSysGetHdmiMonitorInfo(type, info.data(), size);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == expected) return true;
        std::fprintf(stderr, "threw: %s\n", error.what());
        return false;
    }
    return false;
}

}

int main() {
    std::array<std::uint8_t, infoSize + guardSize> buffer{};
    buffer.fill(untouched);
    Require(sceAudioOutSysGetHdmiMonitorInfo(monitorTypeHdmi, buffer.data(), infoSize) == 0,
        "an HDMI query with a 0x180-byte buffer must succeed");
    for (std::uint32_t i = 0; i < infoSize; i++) {
        Require(buffer[i] == 0, "the monitor info must describe a sink without a name or audio descriptors");
    }
    std::uint32_t descriptors = 0xFFFFFFFFu;
    std::memcpy(&descriptors, buffer.data() + descriptorCountOffset, sizeof(descriptors));
    Require(descriptors == 0, "the audio descriptor count must be 0");
    for (std::size_t i = infoSize; i < buffer.size(); i++) {
        Require(buffer[i] == untouched, "nothing past 0x180 bytes may be written");
    }

    Require(sceAudioOutSysGetHdmiMonitorInfo(monitorTypeHdmi, nullptr, infoSize) == invalidPointer,
        "a null buffer must report an invalid pointer");

    buffer.fill(untouched);
    for (const std::uint32_t size : {0u, infoSize - 1, infoSize + 1, 0xffffffffu}) {
        Require(sceAudioOutSysGetHdmiMonitorInfo(monitorTypeHdmi, buffer.data(), size) == static_cast<int>(0x80260006), "exact size required");
        Require(sceAudioOutSysGetHdmiMonitorInfo(0, nullptr, size) == invalidPointer, "pointer checked before size and type");
        for (const auto value : buffer) Require(value == untouched, "failure preserves output");
    }
    for (const int type : {0, 2, -1, 17}) Require(sceAudioOutSysGetHdmiMonitorInfo(type, buffer.data(), infoSize) == static_cast<int>(0x8026000C), "unsupported type");
    Require(Throws(0x10, infoSize,
        "sceAudioOutSysGetHdmiMonitorInfo: monitor type 16 not supported"), "unimplemented device query");
    return 0;
}
