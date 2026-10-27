#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template<typename TFunction>
static bool ThrowsException(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

namespace {

static_assert(sizeof(AudioOut2MasteringCompressorStates) == 208);
static_assert(sizeof(AudioOut2MasteringCompressorStatesV2) == 272);
static_assert(sizeof(AudioOut2MasteringLimiterStates) == 112);
static_assert(sizeof(AudioOut2MasteringStates) == 336);
static_assert(sizeof(AudioOut2MasteringStatesV2) == 400);

constexpr int systemUser = 0xFF;
constexpr int user = 0x10000000;
constexpr int errorInvalidParam = static_cast<int>(0x80268001);
constexpr int errorInvalidPointer = static_cast<int>(0x8026800C);
constexpr int errorMasteringInvalidApiParam = static_cast<int>(0x80268201);
constexpr int errorMasteringInvalidStatesId = static_cast<int>(0x80268204);
constexpr int errorMasteringNotInitialized = static_cast<int>(0x80268203);
constexpr int errorMasteringInvalidFlag = static_cast<int>(0x80268205);

void TestSet3DLatency() {
    Require(sceAudioOut2Set3DLatency(systemUser, 0) == errorInvalidParam, "output 0 for selector 255 must be refused");
    for (std::uint32_t output = 0; output <= 2; ++output) {
        Require(sceAudioOut2Set3DLatency(0, output) == 0, "selector 0 must accept each supported output");
    }
    Require(sceAudioOut2Set3DLatency(systemUser, 1) == 0, "output 1 for the system user must be accepted");
    Require(sceAudioOut2Set3DLatency(systemUser, 2) == 0, "output 2 for the system user must be accepted");
    Require(sceAudioOut2Set3DLatency(systemUser, 2) == 0, "output 2 must be accepted again");
    Require(sceAudioOut2Set3DLatency(user, 1) == errorInvalidParam, "a user id is not a valid latency selector");
    Require(sceAudioOut2Set3DLatency(systemUser, 3) == errorInvalidParam, "output 3 must be refused");
    Require(sceAudioOut2Set3DLatency(systemUser, 0xFFFFFFFFu) == errorInvalidParam, "a large output must be refused");
}

void TestMasteringInit() {
    const AudioOut2MasteringParamsHeader params{1u};
    AudioOut2MasteringStates storage{};
    Require(sceAudioOut2MasteringTerm() == errorMasteringNotInitialized, "termination before initialization is refused");
    Require(sceAudioOut2MasteringSetParam(&params, 0, 0) == errorMasteringNotInitialized, "parameters before initialization are refused");
    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == errorMasteringNotInitialized, "the initialization check comes before the null check");
    Require(sceAudioOut2MasteringGetState(&storage.states_header, 0, 0) == errorMasteringNotInitialized, "states before initialization are refused");
    Require(sceAudioOut2MasteringInit(1) == errorMasteringInvalidApiParam, "flags 1 are refused");
    Require(sceAudioOut2MasteringInit(0x80000000u) == errorMasteringInvalidApiParam, "sceAudioOut2MasteringInit accepts no flag");
    Require(sceAudioOut2MasteringInit(0) == 0, "flags 0 must be accepted");
    Require(sceAudioOut2MasteringInit(1) == 0, "an initialized stage returns 0 before the flags are checked");
    Require(sceAudioOut2MasteringInit(0) == 0, "a second initialization must be accepted");
}

void TestMasteringTerm() {
    Require(sceAudioOut2MasteringTerm() == 0, "termination must be accepted");
    Require(sceAudioOut2MasteringTerm() == errorMasteringNotInitialized, "initializations are not counted");
    Require(sceAudioOut2MasteringInitEx(1) == errorMasteringInvalidApiParam, "sceAudioOut2MasteringInitEx refuses the low bits");
    Require(sceAudioOut2MasteringInitEx(0x80000001u) == errorMasteringInvalidApiParam, "sceAudioOut2MasteringInitEx refuses the low bits with the mode bit");
    Require(sceAudioOut2MasteringInitEx(0x80000000u) == 0, "sceAudioOut2MasteringInitEx accepts the mode bit");
    Require(sceAudioOut2MasteringTerm() == 0, "termination after sceAudioOut2MasteringInitEx must be accepted");
    Require(sceAudioOut2MasteringInitEx(0) == 0, "sceAudioOut2MasteringInitEx accepts flags 0");
}

void TestMasteringSetParam() {
    const AudioOut2MasteringParamsHeader params{1u};
    Require(sceAudioOut2MasteringSetParam(&params, 0, 0) == 0, "mastering parameters must be accepted");
    Require(sceAudioOut2MasteringSetParam(&params, 2, 0) == 0, "mastering parameters for output 2 must be accepted");
    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 0) == errorMasteringInvalidApiParam, "null mastering parameters return the invalid API parameter error");
    Require(sceAudioOut2MasteringSetParam(nullptr, 0, 2) == errorMasteringInvalidApiParam, "the null check comes before the flag check");
    Require(sceAudioOut2MasteringSetParam(&params, 0, 1) == 0, "flags 1 must be accepted");
    Require(sceAudioOut2MasteringSetParam(&params, 0, 2) == errorMasteringInvalidFlag, "flags above 1 are refused");
}

void TestMasteringGetState() {
    alignas(16) std::uint8_t storage[512];
    Require(sceAudioOut2MasteringGetState(nullptr, 0, 0) == errorInvalidPointer, "a null state returns the invalid pointer error");
    auto* header = reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage);
    auto* words = reinterpret_cast<std::uint32_t*>(storage);
    header->states_id = 1u;
    Require(sceAudioOut2MasteringGetState(header, 0, 0) == 0, "states id 1 must be accepted");
    Require(header->states_id == 1u, "states id 1 is kept");
    Require(words[4] == 0x01010001u && words[5] == 8u + 8u + 3u * 8u * 4u * 2u, "the compressor states descriptor is filled");
    Require(words[56] == 0x01010003u && words[57] == 8u + 8u + 8u * 4u * 3u, "the limiter states descriptor is filled");
    std::fill(storage, storage + sizeof(storage), 0xA5);
    header->states_id = 2u;
    Require(sceAudioOut2MasteringGetState(header, 2, 1) == 0, "states id 2 must be accepted");
    Require(header->states_id == 2u, "states id 2 is kept");
    Require(words[4] == 0x01020001u && words[5] == 8u + 8u + 4u * 8u * 4u * 2u, "the V2 compressor states descriptor is filled");
    Require(words[72] == 0x01010003u && words[73] == 8u + 8u + 8u * 4u * 3u, "the V2 limiter states descriptor is filled");
    Require(std::all_of(storage + 8, storage + 16, [](std::uint8_t byte) { return byte == 0; }), "the reserved words are zeroed");
    Require(std::all_of(storage + 24, storage + 288, [](std::uint8_t byte) { return byte == 0; }), "the V2 compressor states are zeroed");
    header->states_id = 3u;
    Require(sceAudioOut2MasteringGetState(header, 0, 0) == errorMasteringInvalidStatesId, "an unknown states id is refused");
    header->states_id = 0u;
    Require(sceAudioOut2MasteringGetState(header, 0, 0) == errorMasteringInvalidStatesId, "states id 0 is refused");
}

void TestLegacyMasteringInit() {
    Require(sceAudioOutMasteringInit(0) == 0, "legacy flags 0 must be accepted");
    Require(ThrowsException([] { sceAudioOutMasteringInit(1); }), "legacy flags SwapEqCompressor must throw");
    Require(sceAudioOutMasteringInit(0) == 0, "a repeated legacy initialization must be accepted");
    Require(ThrowsException([] { sceAudioOutMasteringInit(2); }), "legacy flags bit 1 must throw");
    Require(ThrowsException([] { sceAudioOutMasteringInit(3); }), "legacy flags bits 0 and 1 must throw");
    Require(ThrowsException([] { sceAudioOutMasteringInit(0x80000000u); }), "legacy flags bit 31 must throw");
    Require(ThrowsException([] { sceAudioOutMasteringInit(0xFFFFFFFFu); }), "legacy flags 0xFFFFFFFF must throw");
}

void TestLegacyMasteringTerm() {
    Require(sceAudioOutMasteringTerm() == 0, "legacy termination must be accepted");
    Require(sceAudioOutMasteringTerm() == 0, "legacy termination must be accepted for each initialization");
}

void TestLegacyMasteringSetParam() {
    alignas(16) std::uint32_t params[16];
    std::fill(params, params + 16, 0xA5A5A5A5u);
    params[0] = 1u;
    Require(sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 0) == 0, "legacy mastering parameters must be accepted");
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 1); }), "legacy parameters with SwapEqCompressor must throw");
    Require(params[0] == 1u && params[1] == 0xA5A5A5A5u && params[15] == 0xA5A5A5A5u, "legacy mastering parameters are not modified");
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 2); }), "legacy parameters with flags bit 1 must throw");
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 0xFFFFFFFFu); }), "legacy parameters with unknown flags must throw");
    Require(ThrowsException([] { sceAudioOutMasteringSetParam(nullptr, 0); }), "null legacy parameters must throw");
    Require(ThrowsException([] { sceAudioOutMasteringSetParam(nullptr, 2); }), "null legacy parameters with unknown flags must throw");
    params[0] = 0u;
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 0); }), "legacy params id 0 must throw");
    params[0] = 2u;
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 0); }), "legacy params id 2 must throw");
    params[0] = 0x01010001u;
    Require(ThrowsException([&] { sceAudioOutMasteringSetParam(reinterpret_cast<const AudioOut2MasteringParamsHeader*>(params), 0); }), "an unknown legacy params id must throw");
}

void TestLegacyMasteringGetState() {
    constexpr std::size_t size = 512;
    constexpr std::size_t statesSize = 336;
    alignas(16) std::uint8_t storage[size];
    auto* header = reinterpret_cast<std::uint32_t*>(storage);
    std::fill(storage, storage + size, 0xA5);
    *header = 1u;
    Require(sceAudioOutMasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage)) == 0, "legacy states id 1 must be accepted");
    Require(*header == 1u, "legacy states id 1 is kept");
    const std::pair<std::size_t, std::size_t> meters[] = {{32, 128}, {128, 224}, {240, 272}, {272, 304}, {304, 336}};
    for (const auto& meter : meters) {
        Require(std::all_of(storage + meter.first, storage + meter.second, [](std::uint8_t byte) { return byte == 0; }), "the legacy mastering meters are zeroed");
    }
    Require(std::all_of(storage + 4, storage + 32, [](std::uint8_t byte) { return byte == 0xA5; }), "the bytes before the compressor meters are not modified");
    Require(std::all_of(storage + 224, storage + 240, [](std::uint8_t byte) { return byte == 0xA5; }), "the limiter descriptor bytes are not modified");
    Require(std::all_of(storage + statesSize, storage + size, [](std::uint8_t byte) { return byte == 0xA5; }), "the bytes after the states are not modified");
    Require(sceAudioOutMasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage)) == 0, "legacy states id 1 must be accepted again");

    for (std::uint32_t id : {0u, 2u, 3u, 0x01010001u, 0xFFFFFFFFu}) {
        std::fill(storage, storage + size, 0xA5);
        *header = id;
        Require(ThrowsException([&] { sceAudioOutMasteringGetState(reinterpret_cast<AudioOut2MasteringStatesHeader*>(storage)); }), "an unknown legacy states id must throw");
        Require(*header == id && std::all_of(storage + 4, storage + size, [](std::uint8_t byte) { return byte == 0xA5; }), "a refused legacy states id leaves the buffer unmodified");
    }
    Require(ThrowsException([] { sceAudioOutMasteringGetState(nullptr); }), "a null legacy state must throw");
}

void TestSetSystemDebugState() {
    std::uint32_t loudness[2] = {1, 0};
    const auto set = [&loudness](std::uint32_t id, void* data, std::size_t size) {
        const AudioOut2SystemDebugStateParam param{id, 0, data, size};
        return sceAudioOut2SetSystemDebugState(&param);
    };
    Require(set(1, loudness, 8) == 0, "the loudness debug state must be accepted");
    Require(set(1, loudness, 8) == 0, "the loudness debug state must be accepted again");
    Require(set(1, loudness, 0) == errorInvalidParam, "a loudness debug state of size 0 is refused");
    Require(set(1, loudness, 4) == errorInvalidParam, "a loudness debug state of size 4 is refused");
    Require(set(1, loudness, 16) == errorInvalidParam, "a loudness debug state of size 16 is refused");
    Require(set(0, loudness, 8) == errorInvalidParam, "debug state id 0 is refused");
    Require(set(2, loudness, 8) == errorInvalidParam, "debug state id 2 is refused");
    Require(set(0x21, loudness, 8) == errorInvalidParam, "debug state id 0x21 is refused");
    Require(set(0xFFFFFFFFu, loudness, 8) == errorInvalidParam, "a large debug state id is refused");
    Require(set(0x20, loudness, 8) == errorInvalidParam, "debug state id 0x20 of size 8 is refused");
    Require(set(0x20, loudness, 0) == errorInvalidParam, "debug state id 0x20 of size 0 is refused");
    Require(ThrowsRuntimeError([&] { set(0x20, loudness, 4); }), "debug state id 0x20 of size 4 must throw");
    Require(ThrowsRuntimeError([&] { set(1, nullptr, 8); }), "a loudness debug state without a param must throw");
    Require(sceAudioOut2SetSystemDebugState(nullptr) == errorInvalidPointer, "a null debug state is refused");
}

void TestEnableChat() {
    Require(sceAudioOut2EnableChat() == 0, "enabling chat must be accepted");
    Require(sceAudioOut2EnableChat() == 0, "enabling chat again must be accepted");
    Require(sceAudioOut2DisableChat() == 0, "disabling chat must be accepted");
    Require(sceAudioOut2DisableChat() == 0, "disabling chat again must be accepted");
    Require(sceAudioOut2EnableChat() == 0, "enabling chat after disabling it must be accepted");
}

void TestLowLatencyService() {
    Require(sceAudioOut2LoInit() == 0, "starting the low-latency service must be accepted");
    Require(sceAudioOut2LoInit() == 0, "starting the low-latency service again must be accepted");
    Require(sceAudioOut2LoTerminate() == 0, "stopping the low-latency service returns 0");
    Require(sceAudioOut2LoTerminate() == 0, "stopping a stopped low-latency service returns 0");
    Require(sceAudioOut2InitializeEx(0) == 0 && sceAudioOut2InitializeEx(1) == 0, "the extended initialization succeeds");
}

}

int main() {
    Require(sceAudioOut2Initialize() == 0, "initialization must succeed");
    Require(sceAudioOut2Initialize() == static_cast<int>(0x80268004), "a second AudioOut2 initialization fails");
    Require(sceAudioOutInit() == 0, "the first legacy initialization succeeds after AudioOut2's");
    Require(sceAudioOut2Initialize() == static_cast<int>(0x80268004), "AudioOut2 initialization fails after the legacy one");
    Require(sceAudioOutInit() == static_cast<int>(0x8026000e), "a second legacy initialization fails");
    TestSet3DLatency();
    TestMasteringInit();
    TestMasteringTerm();
    TestMasteringSetParam();
    TestMasteringGetState();
    TestLegacyMasteringInit();
    TestLegacyMasteringTerm();
    TestLegacyMasteringSetParam();
    TestLegacyMasteringGetState();
    TestSetSystemDebugState();
    TestEnableChat();
    TestLowLatencyService();
    return 0;
}
