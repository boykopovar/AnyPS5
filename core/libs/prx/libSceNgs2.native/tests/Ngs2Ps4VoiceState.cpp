#include "Ngs2Test.hpp"
#include "prx/libSceNgs2.native/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>

static bool Untouched(const std::array<unsigned char, 256>& buffer) {
    return std::all_of(buffer.begin(), buffer.end(), [](auto byte) { return byte == 0xaa; });
}

static void InvalidVoice(uintptr_t voice) {
    std::uint32_t flags = 0xaaaaaaaa;
    Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_ERROR_PS4_INVALID_VOICE_HANDLE && flags == 0);
    for (const std::size_t size : {std::size_t{64}, std::size_t{4}, std::size_t{0}, std::size_t{3}}) {
        alignas(16) std::array<unsigned char, 256> buffer;
        buffer.fill(0xaa);
        const int expected = size < 4 ? SCE_NGS2_ERROR_PS4_INVALID_STATE_SIZE : SCE_NGS2_ERROR_PS4_INVALID_VOICE_HANDLE;
        Require(sceNgs2VoiceGetState(voice, reinterpret_cast<Ngs2VoiceState*>(buffer.data()), size) == expected);
        Require(Untouched(buffer));
    }
}

int main() {
    try {
        alignas(16) static std::array<unsigned char, 256> zeroedVoice{};
        InvalidVoice(reinterpret_cast<uintptr_t>(zeroedVoice.data()));
        InvalidVoice(0x1234);
        const auto system = CreateSystem();
        const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
        const auto voice = Voice(rack);
        std::uint32_t flags = 0xaaaaaaaa;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK && flags == 0);
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        InvalidVoice(voice);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        std::cout << "NGS2 PS4 voice state errors checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
