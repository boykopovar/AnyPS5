#include "prx/libSceOpusSilkEnc/include/Api.hpp"
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
OpusSilkEncoder* ToState(std::array<std::uint8_t, Size>& block) {
    return reinterpret_cast<OpusSilkEncoder*>(block.data());
}

void SetWord(std::uint8_t* data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data + offset, &value, sizeof value);
}

int EncoderError(std::uint32_t offset) {
    return static_cast<int>(0x81770000u + offset);
}

} // namespace

int main() {
    constexpr std::uint32_t markerError = 0x15u;
    std::array<std::uint8_t, 1024> blank{};
    std::array<std::uint8_t, 1024> wideMarker{};
    SetWord(wideMarker.data(), 0xE8, 0x000117B2u);
    SetWord(wideMarker.data(), 0x14, 48000u);
    SetWord(wideMarker.data(), 0x18, 2u);
    std::array<std::int16_t, 5760> pcm{};
    std::array<float, 5760> floatPcm{};
    std::array<std::uint8_t, 1500> packet{};

    Require(sceOpusSilkEncEncode(ToState(blank), pcm.data(), 960, nullptr, 1500) == EncoderError(0x04));
    Require(sceOpusSilkEncEncode(ToState(blank), pcm.data(), 960, packet.data(), 0) == EncoderError(0x05));
    Require(sceOpusSilkEncEncode(ToState(blank), pcm.data(), 0, nullptr, 0) == EncoderError(0x04));
    Require(sceOpusSilkEncEncode(ToState(blank), pcm.data(), 0, packet.data(), 0) == EncoderError(0x05));
    Require(sceOpusSilkEncEncode(ToState(blank), pcm.data(), 0, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusSilkEncEncode(ToState(blank), nullptr, 0, nullptr, 0) == EncoderError(0x02));
    Require(sceOpusSilkEncEncode(ToState(wideMarker), pcm.data(), 960, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusSilkEncEncodeFloat(ToState(wideMarker), floatPcm.data(), 960, packet.data(), 1500) == EncoderError(markerError));
    Require(sceOpusSilkEncEncodeFloat(ToState(blank), floatPcm.data(), 0, nullptr, 0) == EncoderError(0x03));
    Require(sceOpusSilkEncDestroy(ToState(wideMarker)) == EncoderError(markerError));

    std::array<std::uint8_t, 1024> live{};
    Require(sceOpusSilkEncCreate(ToState(live), 48000, 2, OPUS_APPLICATION_VOIP) == 0);
    Require(sceOpusSilkEncEncode(ToState(live), pcm.data(), 0, packet.data(), 1500) == EncoderError(0x03));
    Require(sceOpusSilkEncEncode(ToState(live), pcm.data(), 960, nullptr, 1500) == EncoderError(0x04));
    Require(sceOpusSilkEncEncode(ToState(live), pcm.data(), 960, packet.data(), 1500) > 0);
    Require(sceOpusSilkEncDestroy(ToState(live)) == 0);
    Require(sceOpusSilkEncDestroy(ToState(live)) == EncoderError(markerError));
}
