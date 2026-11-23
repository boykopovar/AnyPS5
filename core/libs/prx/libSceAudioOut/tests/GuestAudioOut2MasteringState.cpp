#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int AudioOut2ErrorInvalidPointer = static_cast<int>(0x8026800C);
constexpr int AudioOut2ErrorMasteringInvalidStatesId = static_cast<int>(0x80268204);
constexpr std::uint32_t StatesIdDefault = 1;
constexpr std::uint32_t StatesIdV2 = 2;
constexpr std::uint32_t CompressorDefaultId = 0x01010001;
constexpr std::uint32_t CompressorV2Id = 0x01020001;
constexpr std::uint32_t LimiterDefaultId = 0x01010003;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "audio out2 mastering state: %s\n", message);
    std::exit(1);
}

}

int main() {
    Require(sceAudioOut2Initialize() == 0, "AudioOut2 initializes");
    Require(sceAudioOut2MasteringInit(0) == 0, "mastering initializes");

    AudioOut2MasteringStates state{};
    state.states_header.states_id = StatesIdDefault;
    Require(sceAudioOut2MasteringGetState(&state.states_header, 0, 0) == 0, "the default state is answered");
    Require(state.states_header.states_id == StatesIdDefault, "the default state keeps its id");
    Require(state.compressor_states.descriptor.id == CompressorDefaultId, "the default compressor descriptor is written");
    Require(state.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStates), "the default compressor size is written");
    Require(state.limiter_states.descriptor.id == LimiterDefaultId, "the limiter descriptor is written");

    AudioOut2MasteringStatesV2 stateV2{};
    stateV2.states_header.states_id = StatesIdV2;
    Require(sceAudioOut2MasteringGetState(&stateV2.states_header, 0, 0) == 0, "the V2 state is answered");
    Require(stateV2.states_header.states_id == StatesIdV2, "the V2 state keeps its id");
    Require(stateV2.compressor_states.descriptor.id == CompressorV2Id, "the V2 compressor descriptor is written");
    Require(stateV2.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStatesV2), "the V2 compressor size is written");

    AudioOut2MasteringStates unknown{};
    unknown.states_header.states_id = 99;
    Require(sceAudioOut2MasteringGetState(&unknown.states_header, 0, 0) == AudioOut2ErrorMasteringInvalidStatesId, "an unknown state id is rejected");
    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == AudioOut2ErrorInvalidPointer, "a null state is rejected");

    errno = EBUSY;
    Require(sceAudioOut2MasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(std::uintptr_t{0x10}), 0, 0) == AudioOut2ErrorInvalidPointer,
            "an unreadable state is rejected");
    Require(errno == EBUSY, "an unreadable state preserves errno");

    state.states_header.states_id = StatesIdDefault;
    Require(sceAudioOut2MasteringGetState(&state.states_header, 0, 0) == 0, "a valid query after a rejected one still answers");
    Require(sceAudioOut2MasteringTerm() == 0, "mastering terminates");
    Require(sceAudioOut2Terminate() == 0, "AudioOut2 terminates");
}
