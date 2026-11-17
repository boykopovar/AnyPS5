#include "prx/libSceOpusCeltEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <opus.h>

namespace {

void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at %s:%d\n", location.file_name(), static_cast<int>(location.line()));
        std::abort();
    }
}

template <std::size_t Size>
OpusCeltEncoder* ToState(std::array<std::uint8_t, Size>& block) {
    return reinterpret_cast<OpusCeltEncoder*>(block.data());
}

void SetWord(std::uint8_t* data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data + offset, &value, sizeof value);
}

int EncoderError(std::uint32_t offset) {
    return static_cast<int>(0x813F0000u + offset);
}

} // namespace

int main() {
    constexpr std::uint32_t markerError = 0x16u;
    std::array<std::uint8_t, 1024> blank{};
    std::array<std::uint8_t, 1024> wideMarker{};
    SetWord(wideMarker.data(), 0xCC, 0x000117B2u);
    SetWord(wideMarker.data(), 0x14, 48000u);
    SetWord(wideMarker.data(), 0x10, 2u);
    std::array<std::int16_t, 5760> pcm{};
    std::array<float, 5760> floatPcm{};
    std::array<std::uint8_t, 1500> packet{};

    Require(sceOpusCeltEncEncode(ToState(blank), pcm.data(), 960, nullptr, 1500) == EncoderError(0x04));
    Require(sceOpusCeltEncEncode(ToState(blank), pcm.data(), 960, packet.data(), 0) == EncoderError(0x05));
    Require(sceOpusCeltEncEncode(ToState(blank), pcm.data(), 0, nullptr, 0) == EncoderError(0x04));
    Require(sceOpusCeltEncEncode(ToState(blank), pcm.data(), 0, packet.data(), 0) == EncoderError(0x05));
    Require(sceOpusCeltEncEncode(ToState(blank), pcm.data(), 0, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusCeltEncEncode(ToState(blank), nullptr, 0, nullptr, 0) == EncoderError(0x02));
    Require(sceOpusCeltEncEncode(ToState(wideMarker), pcm.data(), 960, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusCeltEncEncodeFloat(ToState(wideMarker), floatPcm.data(), 960, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusCeltEncEncodeFloat(ToState(blank), floatPcm.data(), 0, nullptr, 0) == EncoderError(0x03));
    Require(sceOpusCeltEncDestroy(ToState(wideMarker)) == EncoderError(markerError));

    std::array<std::uint8_t, 1024> live{};
    Require(sceOpusCeltEncCreate(ToState(live), 48000, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY) == 0);
    Require(sceOpusCeltEncEncode(ToState(live), pcm.data(), 0, packet.data(), 1500) == EncoderError(0x03));
    Require(sceOpusCeltEncEncode(ToState(live), pcm.data(), 960, nullptr, 1500) == EncoderError(0x04));
    Require(sceOpusCeltEncEncode(ToState(live), pcm.data(), 960, packet.data(), 1500) > 0);
    Require(sceOpusCeltEncDestroy(ToState(live)) == 0);
    Require(sceOpusCeltEncDestroy(ToState(live)) == EncoderError(markerError));
}
