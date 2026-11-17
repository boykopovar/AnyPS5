#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

int main() {
    constexpr int masteringInvalidFlags = static_cast<int>(0x80260201);
    constexpr int masteringNotInitialized = static_cast<int>(0x80260203);
    constexpr int masteringInvalidStatesId = static_cast<int>(0x80260204);
    constexpr int masteringInvalidApiParam = static_cast<int>(0x80268201);
    constexpr int masteringNotInitialized2 = static_cast<int>(0x80268203);
    constexpr int masteringInvalidStatesId2 = static_cast<int>(0x80268204);
    constexpr int masteringInvalidFlag2 = static_cast<int>(0x80268205);

    Require(sceAudioOutInit() == 0);

    Require(sceAudioOutMasteringTerm() == masteringNotInitialized);
    {
        std::array<unsigned char, sizeof(AudioOut2MasteringStates)> state{};
        auto* header = reinterpret_cast<AudioOut2MasteringStatesHeader*>(state.data());
        header->states_id = 1;
        Require(sceAudioOutMasteringGetState(header) == masteringNotInitialized);
        header->states_id = 2;
        Require(sceAudioOutMasteringGetState(header) == masteringInvalidStatesId);
    }

    Require(sceAudioOutMasteringInit(1) == masteringInvalidFlags);
    ExpectErr([] { sceAudioOutMasteringInit(2); });
    Require(sceAudioOutMasteringInit(0) == 0);
    Require(sceAudioOutMasteringInit(0) == 0);

    {
        std::array<unsigned char, sizeof(AudioOut2MasteringStates)> state{};
        std::memset(state.data(), 0xaa, state.size());
        auto* header = reinterpret_cast<AudioOut2MasteringStatesHeader*>(state.data());
        header->states_id = 1;
        Require(sceAudioOutMasteringGetState(header) == 0);
        auto* full = reinterpret_cast<AudioOut2MasteringStates*>(state.data());
        for (float value : full->compressor_states.input_rms[0]) Require(value == 0.0f);
        for (float value : full->compressor_states.compression_coeff[2]) Require(value == 0.0f);
        for (float value : full->limiter_states.input_peak) Require(value == 0.0f);
        for (float value : full->limiter_states.output_peak) Require(value == 0.0f);
        for (float value : full->limiter_states.gain_peak) Require(value == 0.0f);
        Require(full->states_header.states_id == 1);
        Require(full->compressor_states.descriptor.id != 0);
        Require(full->compressor_states.descriptor.size != 0);
    }

    {
        AudioOut2MasteringParamsHeader param{1};
        Require(sceAudioOutMasteringSetParam(&param, 0) == 0);
        ExpectErr([] { AudioOut2MasteringParamsHeader other{5}; sceAudioOutMasteringSetParam(&other, 0); });
        ExpectErr([] { sceAudioOutMasteringSetParam(nullptr, 0); });
    }

    Require(sceAudioOutMasteringTerm() == 0);
    Require(sceAudioOutMasteringTerm() == masteringNotInitialized);
    {
        std::array<unsigned char, sizeof(AudioOut2MasteringStates)> state{};
        auto* header = reinterpret_cast<AudioOut2MasteringStatesHeader*>(state.data());
        header->states_id = 1;
        Require(sceAudioOutMasteringGetState(header) == masteringNotInitialized);
    }

    {
        AudioOut2MasteringStatesHeader header{1};
        AudioOut2MasteringParamsHeader param{1};
        Require(sceAudioOut2MasteringTerm() == masteringNotInitialized2);
        Require(sceAudioOut2MasteringGetState(&header, 0, 0x10000000u) == masteringNotInitialized2);
        Require(sceAudioOut2MasteringSetParam(&param, 0, 0) == masteringNotInitialized2);
        Require(sceAudioOut2MasteringInit(2) == masteringInvalidApiParam);
    }

    Require(sceAudioOut2MasteringInitEx(0) == 0);
    Require(sceAudioOut2MasteringInit(2) == 0);

    {
        AudioOut2MasteringStates state{};
        std::memset(&state, 0xaa, sizeof(state));
        state.states_header.states_id = 1;
        Require(sceAudioOut2MasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(&state), 0, 0x10000000u) == 0);
        Require(state.states_header.states_id == 1);
        Require(state.compressor_states.descriptor.id == static_cast<std::uint32_t>(0x01010001));
        Require(state.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStates));
        Require(state.limiter_states.descriptor.id == static_cast<std::uint32_t>(0x01010003));
        Require(state.limiter_states.descriptor.size == sizeof(AudioOut2MasteringLimiterStates));
        for (float value : state.compressor_states.input_rms[1]) Require(value == 0.0f);
        for (float value : state.limiter_states.gain_peak) Require(value == 0.0f);
    }

    {
        AudioOut2MasteringStatesV2 state{};
        std::memset(&state, 0xaa, sizeof(state));
        state.states_header.states_id = 2;
        Require(sceAudioOut2MasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(&state), 0, 0x10000000u) == 0);
        Require(state.compressor_states.descriptor.id == static_cast<std::uint32_t>(0x01020001));
        Require(state.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStatesV2));
        Require(state.limiter_states.descriptor.id == static_cast<std::uint32_t>(0x01010003));
    }

    {
        AudioOut2MasteringStatesHeader other{3};
        Require(sceAudioOut2MasteringGetState(&other, 0, 0x10000000u) == masteringInvalidStatesId2);
        Require(sceAudioOut2MasteringGetState(nullptr, 0, 0x10000000u) == static_cast<int>(0x8026800C));
        AudioOut2MasteringParamsHeader param{1};
        Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == masteringInvalidApiParam);
        Require(sceAudioOut2MasteringSetParam(&param, 0, 2) == masteringInvalidFlag2);
        Require(sceAudioOut2MasteringSetParam(&param, 0, 1) == 0);
    }

    Require(sceAudioOut2MasteringTerm() == 0);
    Require(sceAudioOut2MasteringTerm() == masteringNotInitialized2);
}
