#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x80268006);
constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800C);
constexpr int ErrorMasteringNotInitialized = static_cast<int>(0x80268203);
constexpr int ErrorMasteringInvalidApiParam = static_cast<int>(0x80268201);
constexpr int ErrorMasteringInvalidStatesId = static_cast<int>(0x80268204);
constexpr int ErrorMasteringInvalidFlag = static_cast<int>(0x80268205);

constexpr std::uint32_t MasteringStatesIdDefault = 1;
constexpr std::uint32_t MasteringStatesIdV2 = 2;
constexpr std::uint32_t MasteringStructIdCompressorDefault = 0x01010001;
constexpr std::uint32_t MasteringStructIdCompressorV2 = 0x01020001;
constexpr std::uint32_t MasteringStructIdLimiterDefault = 0x01010003;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedAudioOut2() {
    AudioOut2MasteringStates states{};
    states.states_header.states_id = MasteringStatesIdDefault;
    Require(sceAudioOut2MasteringInit(0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringInitEx(0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringTerm() == ErrorNotInitialized);
    Require(sceAudioOut2MasteringGetState(&states.states_header, 0, 0) == ErrorNotInitialized);

    AudioOut2MasteringParamsHeader paramHeader{1};
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 0) == ErrorNotInitialized);
}

void TestMasteringLifecycleAndValidation() {
    Require(sceAudioOut2Initialize() == 0);

    Require(sceAudioOut2MasteringTerm() == ErrorMasteringNotInitialized);

    Require(sceAudioOut2MasteringInit(0xFFFFFFFFu) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringInitEx(0x7FFFFFFFu) == ErrorMasteringInvalidApiParam);

    Require(sceAudioOut2MasteringInit(0) == 0);
    Require(sceAudioOut2MasteringInit(0) == 0);

    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == ErrorInvalidPointer);

    AudioOut2MasteringStatesHeader invalidHeader{99};
    Require(sceAudioOut2MasteringGetState(&invalidHeader, 0, 0) == ErrorMasteringInvalidStatesId);

    AudioOut2MasteringStates statesV1{};
    statesV1.states_header.states_id = MasteringStatesIdDefault;
    Require(sceAudioOut2MasteringGetState(&statesV1.states_header, 0, 0) == 0);
    Require(statesV1.compressor_states.descriptor.id == MasteringStructIdCompressorDefault);
    Require(statesV1.limiter_states.descriptor.id == MasteringStructIdLimiterDefault);

    AudioOut2MasteringStatesV2 statesV2{};
    statesV2.states_header.states_id = MasteringStatesIdV2;
    Require(sceAudioOut2MasteringGetState(&statesV2.states_header, 0, 0) == 0);
    Require(statesV2.compressor_states.descriptor.id == MasteringStructIdCompressorV2);
    Require(statesV2.limiter_states.descriptor.id == MasteringStructIdLimiterDefault);

    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == ErrorMasteringInvalidApiParam);

    AudioOut2MasteringParamsHeader paramHeader{1};
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 99) == ErrorMasteringInvalidFlag);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 0) == 0);

    Require(sceAudioOut2MasteringTerm() == 0);
    Require(sceAudioOut2MasteringTerm() == ErrorMasteringNotInitialized);
}

} // namespace

int main() {
    TestUninitializedAudioOut2();
    TestMasteringLifecycleAndValidation();
    return 0;
}
