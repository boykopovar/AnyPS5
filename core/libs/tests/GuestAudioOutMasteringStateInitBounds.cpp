#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

constexpr int ErrorInvalidPort = static_cast<int>(0x80260003u);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260004u);

constexpr int ErrorNotInitialized = static_cast<int>(0x80268006u);
constexpr int ErrorInvalidPointer2 = static_cast<int>(0x8026800Cu);
constexpr int ErrorMasteringNotInitialized = static_cast<int>(0x80268203u);
constexpr int ErrorMasteringInvalidApiParam = static_cast<int>(0x80268201u);
constexpr int ErrorMasteringInvalidStatesId = static_cast<int>(0x80268204u);
constexpr int ErrorMasteringInvalidFlag = static_cast<int>(0x80268205u);
constexpr int ErrorMasteringInvalidFlags = static_cast<int>(0x80260201u);
constexpr int ErrorInvalidHandle2 = static_cast<int>(0x80260503u);

constexpr std::uint32_t MasteringStatesIdDefault = 1;
constexpr std::uint32_t MasteringStatesIdV2 = 2;
constexpr std::uint32_t MasteringStructIdCompressorDefault = 0x01010001;
constexpr std::uint32_t MasteringStructIdCompressorV2 = 0x01020001;
constexpr std::uint32_t MasteringStructIdLimiterDefault = 0x01010003;

void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", location.line());
        std::fflush(stderr);
        std::abort();
    }
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestLegacyMastering() {
    Require(sceAudioOutMasteringInit(0) == 0);
    Require(sceAudioOutMasteringInit(1) == ErrorMasteringInvalidFlags);
    Require(Throws([] { sceAudioOutMasteringInit(0x80000000u); }));
    Require(Throws([] { sceAudioOutMasteringInit(0xFFFFFFFFu); }));

    Require(sceAudioOutMasteringTerm() == 0);

    Require(Throws([] { sceAudioOutMasteringGetState(nullptr); }));

    alignas(16) std::uint8_t storage[512];
    auto* header = reinterpret_cast<std::uint32_t*>(storage);

    for (std::uint32_t badId : {0u, 2u, 3u, 0x01010001u, 0xFFFFFFFFu}) {
        std::fill(storage, storage + sizeof(storage), 0xA5);
        *header = badId;
        Require(Throws([&] { sceAudioOutMasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage)); }));
        Require(*header == badId);
        Require(std::all_of(storage + 4, storage + sizeof(storage), [](std::uint8_t byte) { return byte == 0xA5; }));
    }

    std::fill(storage, storage + sizeof(storage), 0xA5);
    *header = MasteringStatesIdDefault;
    Require(sceAudioOutMasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage)) == 0);
    Require(*header == MasteringStatesIdDefault);

    const std::pair<std::size_t, std::size_t> zeroedRanges[] = {
        {32, 128}, {128, 224}, {240, 272}, {272, 304}, {304, 336}
    };
    for (const auto& range : zeroedRanges) {
        Require(std::all_of(storage + range.first, storage + range.second, [](std::uint8_t b) { return b == 0; }));
    }
    Require(std::all_of(storage + 4, storage + 32, [](std::uint8_t b) { return b == 0xA5; }));
    Require(std::all_of(storage + 224, storage + 240, [](std::uint8_t b) { return b == 0xA5; }));
    Require(std::all_of(storage + 336, storage + sizeof(storage), [](std::uint8_t b) { return b == 0xA5; }));

    Require(Throws([] { sceAudioOutMasteringSetParam(nullptr, 0); }));
    Require(Throws([] { sceAudioOutMasteringSetParam(nullptr, 1); }));

    std::uint32_t paramStorage[16];
    std::fill(std::begin(paramStorage), std::end(paramStorage), 0xA5A5A5A5u);
    paramStorage[0] = 1u;

    Require(sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(paramStorage), 0) == 0);
    Require(paramStorage[0] == 1u);
    Require(paramStorage[1] == 0xA5A5A5A5u);

    Require(Throws([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(paramStorage), 1); }));
    Require(Throws([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(paramStorage), 2); }));
    Require(Throws([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(paramStorage), 0xFFFFFFFFu); }));

    for (std::uint32_t badParamId : {0u, 2u, 3u, 0x01010001u}) {
        paramStorage[0] = badParamId;
        Require(Throws([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(paramStorage), 0); }));
    }
}

void TestAudioOut2Mastering() {
    AudioOut2MasteringStates states{};
    states.states_header.states_id = MasteringStatesIdDefault;
    AudioOut2MasteringParamsHeader paramHeader{1};

    Require(sceAudioOut2MasteringInit(0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringInit(1) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringInitEx(0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringInitEx(0x80000000u) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringTerm() == ErrorNotInitialized);
    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringGetState(&states.states_header, 0, 0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == ErrorNotInitialized);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 0) == ErrorNotInitialized);

    Require(sceAudioOut2Initialize() == 0);

    Require(sceAudioOut2MasteringTerm() == ErrorMasteringNotInitialized);
    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == ErrorMasteringNotInitialized);
    Require(sceAudioOut2MasteringGetState(&states.states_header, 0, 0) == ErrorMasteringNotInitialized);
    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == ErrorMasteringNotInitialized);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 0) == ErrorMasteringNotInitialized);

    Require(sceAudioOut2MasteringInit(1) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringInit(0x80000000u) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringInit(0xFFFFFFFFu) == ErrorMasteringInvalidApiParam);

    Require(sceAudioOut2MasteringInit(0) == 0);
    Require(sceAudioOut2MasteringInit(1) == 0);
    Require(sceAudioOut2MasteringInit(0) == 0);

    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == ErrorInvalidPointer2);

    AudioOut2MasteringStatesHeader invalidHeader{99};
    Require(sceAudioOut2MasteringGetState(&invalidHeader, 0, 0) == ErrorMasteringInvalidStatesId);
    invalidHeader.states_id = 0;
    Require(sceAudioOut2MasteringGetState(&invalidHeader, 0, 0) == ErrorMasteringInvalidStatesId);
    invalidHeader.states_id = 3;
    Require(sceAudioOut2MasteringGetState(&invalidHeader, 0, 0) == ErrorMasteringInvalidStatesId);

    AudioOut2MasteringStates statesV1{};
    statesV1.states_header.states_id = MasteringStatesIdDefault;
    Require(sceAudioOut2MasteringGetState(&statesV1.states_header, 0, 0) == 0);
    Require(statesV1.compressor_states.descriptor.id == MasteringStructIdCompressorDefault);
    Require(statesV1.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStates));
    Require(statesV1.limiter_states.descriptor.id == MasteringStructIdLimiterDefault);
    Require(statesV1.limiter_states.descriptor.size == sizeof(AudioOut2MasteringLimiterStates));

    AudioOut2MasteringStatesV2 statesV2{};
    statesV2.states_header.states_id = MasteringStatesIdV2;
    Require(sceAudioOut2MasteringGetState(&statesV2.states_header, 0, 0) == 0);
    Require(statesV2.compressor_states.descriptor.id == MasteringStructIdCompressorV2);
    Require(statesV2.compressor_states.descriptor.size == sizeof(AudioOut2MasteringCompressorStatesV2));
    Require(statesV2.limiter_states.descriptor.id == MasteringStructIdLimiterDefault);
    Require(statesV2.limiter_states.descriptor.size == sizeof(AudioOut2MasteringLimiterStates));

    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 2) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 2) == ErrorMasteringInvalidFlag);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 99) == ErrorMasteringInvalidFlag);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 0) == 0);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 0, 1) == 0);
    Require(sceAudioOut2MasteringSetParam(&paramHeader, 2, 0) == 0);

    Require(sceAudioOut2MasteringTerm() == 0);
    Require(sceAudioOut2MasteringTerm() == ErrorMasteringNotInitialized);

    Require(sceAudioOut2MasteringInitEx(1) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringInitEx(0x80000001u) == ErrorMasteringInvalidApiParam);
    Require(sceAudioOut2MasteringInitEx(0x80000000u) == 0);
    Require(sceAudioOut2MasteringTerm() == 0);
    Require(sceAudioOut2MasteringInitEx(0) == 0);
    Require(sceAudioOut2MasteringTerm() == 0);
}

void TestUnopenedHandleChecks() {
    for (int handle : {-1, 0, 1, 2, 33, 999}) {
        Require(sceAudioOutClose(handle) == ErrorInvalidPort);

        AudioOutPortState portState{};
        Require(sceAudioOutGetPortState(handle, &portState) == ErrorInvalidPort);
        Require(sceAudioOutGetPortState(handle, nullptr) == ErrorInvalidPointer);

        int vol[2] = {100, 100};
        Require(sceAudioOutSetVolume(handle, 1, vol) == ErrorInvalidPort);
        Require(sceAudioOutSetVolume(handle, 1, nullptr) == ErrorInvalidPointer);

        Require(sceAudioOutSetMixLevelPadSpk(handle, 100) == ErrorInvalidPort);
        Require(sceAudioOutOutput(handle, nullptr) == ErrorInvalidPort);

        AudioOutOutputParam outParam{handle, nullptr};
        Require(sceAudioOutOutputs(&outParam, 1) == ErrorInvalidPort);

        Require(Throws([handle] { sceAudioOutExClose(handle); }));
        Require(Throws([handle] { sceAudioOutSysClose(handle); }));
    }

    AudioOutOutputParam validParam{1, nullptr};
    Require(sceAudioOutOutputs(nullptr, 1) == ErrorInvalidPointer);
    Require(sceAudioOutOutputs(&validParam, 0) == ErrorInvalidPointer);

    for (int handle : {-1, 0, 99}) {
        AudioOut2PortState p2State{};
        Require(sceAudioOut2PortGetState(handle, &p2State) == ErrorInvalidHandle2);
        Require(sceAudioOut2PortDestroy(handle) == ErrorInvalidHandle2);
        Require(sceAudioOut2ContextDestroy(handle) == ErrorInvalidHandle2);
        Require(sceAudioOut2ContextAdvance(handle) == ErrorInvalidHandle2);
        Require(sceAudioOut2ContextPush(handle, 0) == ErrorInvalidHandle2);
        Require(sceAudioOut2ContextSetAttributes(handle, nullptr, 0) == ErrorInvalidHandle2);
    }
}

} // namespace

int main() {
    TestLegacyMastering();
    TestAudioOut2Mastering();
    TestUnopenedHandleChecks();

    LibcRunShutdown_nid_postfix();
    return 0;
}
