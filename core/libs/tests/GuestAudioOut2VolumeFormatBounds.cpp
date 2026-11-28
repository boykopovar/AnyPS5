#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr int errorInvalidHandle = static_cast<int>(0x80260503);
constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
constexpr int errorInvalidParam = static_cast<int>(0x80268001);
constexpr int errorFlagWithout8Channels = static_cast<int>(0x80268001);
constexpr int errorUnsupportedFormat = static_cast<int>(0x8026800E);
constexpr int errorInvalidPort = static_cast<int>(0x80260003);
constexpr int errorInvalidPointer = static_cast<int>(0x80260004);
constexpr int errorInvalidPortType = static_cast<int>(0x8026000A);
constexpr int errorInvalidMixLevel = static_cast<int>(0x80260014);
constexpr std::uint32_t attributeVolume = 1;
constexpr AudioOut2PortHandle deadPortHandle = 0x6B6B;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: check failed\n", location.file_name(), location.line());
        std::abort();
    }
}

AudioOut2ContextHandle InitAndCreateContext() {
    Require(sceAudioOut2Initialize() == 0 || sceAudioOut2Initialize() == static_cast<int>(0x80268004));
    AudioOut2ContextParam ctxParams{};
    Require(sceAudioOut2ContextResetParam(&ctxParams) == 0);
    AudioOut2ContextHandle ctx = 0;
    Require(sceAudioOut2ContextCreate(&ctxParams, nullptr, 0, &ctx) == 0 && ctx != 0);
    return ctx;
}

void TestPortFormatConfigurations(AudioOut2ContextHandle ctx) {
    AudioOut2PortParam param{};
    param.sampling_freq = 48000;

    AudioOut2PortHandle port = 0;
    param.data_format = (1 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    AudioOut2PortState state{};
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 1 && state.output == 1 && state.volume == 127);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (1 << 8) | 1;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 1);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (2 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 2 && state.output == 1 && state.volume == 127);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (2 << 8) | 1;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 2);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (8 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 8 && state.output == 1 && state.volume == 127);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (8 << 8) | 1;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 8);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (8 << 8) | 0x80;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (8 << 8) | 0x81;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (12 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 12 && state.output == 1 && state.volume == 127);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = (12 << 8) | 1;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortGetState(port, &state) == 0 && state.num_channels == 12);
    Require(sceAudioOut2PortDestroy(port) == 0);

    param.data_format = 0xABC00000u | (2 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);
    Require(sceAudioOut2PortDestroy(port) == 0);

    port = 0;
    param.data_format = (6 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorUnsupportedFormat && port == static_cast<AudioOut2PortHandle>(-1));

    port = 0;
    param.data_format = (6 << 8) | 1;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorUnsupportedFormat && port == static_cast<AudioOut2PortHandle>(-1));

    port = 0;
    param.data_format = (1 << 8) | 0x80;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorFlagWithout8Channels && port == static_cast<AudioOut2PortHandle>(-1));

    port = 0;
    param.data_format = (2 << 8) | 0x80;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorFlagWithout8Channels && port == static_cast<AudioOut2PortHandle>(-1));

    port = 0;
    param.data_format = (12 << 8) | 0x80;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorFlagWithout8Channels && port == static_cast<AudioOut2PortHandle>(-1));

    for (const std::uint32_t badType : {2u, 3u, 0x10u, 0x7Fu}) {
        port = 0;
        param.data_format = (2 << 8) | badType;
        Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorUnsupportedFormat && port == static_cast<AudioOut2PortHandle>(-1));
    }

    for (const std::uint32_t badChannels : {0u, 3u, 4u, 5u, 7u, 9u, 10u, 11u, 13u, 14u, 15u}) {
        port = 0;
        param.data_format = (badChannels << 8) | 0;
        Require(sceAudioOut2PortCreate(ctx, &param, &port) == errorUnsupportedFormat && port == static_cast<AudioOut2PortHandle>(-1));
    }

    param.data_format = (2 << 8) | 0;
    Require(sceAudioOut2PortCreate(0, &param, &port) == errorInvalidHandle);
    Require(sceAudioOut2PortCreate(ctx + 0x1000, &param, &port) == errorInvalidHandle);
    Require(sceAudioOut2PortCreate(ctx, nullptr, &port) == errorInvalidArgument);
    Require(sceAudioOut2PortCreate(ctx, &param, nullptr) == errorInvalidArgument);
}

void TestVolumeBoundsAndMuting(AudioOut2ContextHandle ctx) {
    AudioOut2PortParam param{};
    param.sampling_freq = 48000;

    AudioOut2PortHandle monoPort = 0;
    param.data_format = (1 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &monoPort) == 0 && monoPort != 0);

    float vol1[1] = {0.0f};
    AudioOut2Attribute getAttr{};
    getAttr.attribute_id = attributeVolume;
    getAttr.value = vol1;
    getAttr.value_size = sizeof(vol1);
    Require(sceAudioOut2LoPortGetAttributes(monoPort, &getAttr, 1) == 0 && vol1[0] == 1.0f);

    vol1[0] = 0.0f;
    AudioOut2Attribute setAttr{};
    setAttr.attribute_id = attributeVolume;
    setAttr.value = vol1;
    setAttr.value_size = sizeof(vol1);
    Require(sceAudioOut2PortSetAttributes(monoPort, &setAttr, 1) == 0);
    vol1[0] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(monoPort, &getAttr, 1) == 0 && vol1[0] == 0.0f);

    vol1[0] = 1.0f;
    Require(sceAudioOut2PortSetAttributes(monoPort, &setAttr, 1) == 0);
    vol1[0] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(monoPort, &getAttr, 1) == 0 && vol1[0] == 1.0f);

    vol1[0] = 0.5f;
    Require(sceAudioOut2PortSetAttributes(monoPort, &setAttr, 1) == 0);
    vol1[0] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(monoPort, &getAttr, 1) == 0 && vol1[0] == 0.5f);

    Require(sceAudioOut2PortDestroy(monoPort) == 0);

    AudioOut2PortHandle stereoPort = 0;
    param.data_format = (2 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &stereoPort) == 0 && stereoPort != 0);

    float read2[2] = {99.0f, 99.0f};
    getAttr.value = read2;
    getAttr.value_size = sizeof(read2);
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 1.0f && read2[1] == 1.0f);

    float vol2[2] = {0.0f, 0.0f};
    setAttr.value = vol2;
    setAttr.value_size = sizeof(vol2);
    Require(sceAudioOut2PortSetAttributes(stereoPort, &setAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.0f && read2[1] == 0.0f);

    vol2[0] = 1.0f;
    vol2[1] = 0.0f;
    Require(sceAudioOut2PortSetAttributes(stereoPort, &setAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 1.0f && read2[1] == 0.0f);

    vol2[0] = 0.0f;
    vol2[1] = 1.0f;
    Require(sceAudioOut2PortSetAttributes(stereoPort, &setAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.0f && read2[1] == 1.0f);

    vol2[0] = 0.25f;
    vol2[1] = 0.75f;
    Require(sceAudioOut2PortSetAttributes(stereoPort, &setAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.25f && read2[1] == 0.75f);

    float vol3[3] = {0.1f, 0.2f, 0.3f};
    AudioOut2Attribute badAttr{};
    badAttr.attribute_id = attributeVolume;
    badAttr.value = vol3;
    badAttr.value_size = sizeof(vol3);
    Require(sceAudioOut2PortSetAttributes(stereoPort, &badAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.25f && read2[1] == 0.75f);

    badAttr.value_size = sizeof(float);
    Require(sceAudioOut2PortSetAttributes(stereoPort, &badAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.25f && read2[1] == 0.75f);

    badAttr.value_size = 0;
    Require(sceAudioOut2PortSetAttributes(stereoPort, &badAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.25f && read2[1] == 0.75f);

    badAttr.value = nullptr;
    badAttr.value_size = sizeof(vol2);
    Require(sceAudioOut2PortSetAttributes(stereoPort, &badAttr, 1) == 0);
    read2[0] = 99.0f;
    read2[1] = 99.0f;
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &getAttr, 1) == 0 && read2[0] == 0.25f && read2[1] == 0.75f);

    Require(sceAudioOut2PortSetAttributes(deadPortHandle, &setAttr, 1) == errorInvalidHandle);
    Require(sceAudioOut2PortSetAttributes(stereoPort, nullptr, 1) == errorInvalidArgument);
    Require(sceAudioOut2PortSetAttributes(stereoPort, nullptr, 0) == 0);
    Require(sceAudioOut2LoPortGetAttributes(deadPortHandle, &getAttr, 1) == errorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, nullptr, 1) == errorInvalidParam);
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, nullptr, 0) == 0);

    badAttr.attribute_id = 99;
    badAttr.value = read2;
    badAttr.value_size = sizeof(read2);
    Require(sceAudioOut2LoPortGetAttributes(stereoPort, &badAttr, 1) == errorInvalidParam);

    Require(sceAudioOut2PortDestroy(stereoPort) == 0);

    AudioOut2PortHandle port8 = 0;
    param.data_format = (8 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port8) == 0 && port8 != 0);
    std::array<float, 8> vol8 = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 1.0f};
    AudioOut2Attribute set8{};
    set8.attribute_id = attributeVolume;
    set8.value = vol8.data();
    set8.value_size = sizeof(vol8);
    Require(sceAudioOut2PortSetAttributes(port8, &set8, 1) == 0);
    std::array<float, 8> read8{};
    AudioOut2Attribute get8{};
    get8.attribute_id = attributeVolume;
    get8.value = read8.data();
    get8.value_size = sizeof(read8);
    Require(sceAudioOut2LoPortGetAttributes(port8, &get8, 1) == 0);
    for (std::size_t i = 0; i < 8; ++i) {
        Require(read8[i] == vol8[i]);
    }
    Require(sceAudioOut2PortDestroy(port8) == 0);

    AudioOut2PortHandle port12 = 0;
    param.data_format = (12 << 8) | 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port12) == 0 && port12 != 0);
    std::array<float, 12> vol12 = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f, 0.0f};
    AudioOut2Attribute set12{};
    set12.attribute_id = attributeVolume;
    set12.value = vol12.data();
    set12.value_size = sizeof(vol12);
    Require(sceAudioOut2PortSetAttributes(port12, &set12, 1) == 0);
    std::array<float, 12> read12{};
    AudioOut2Attribute get12{};
    get12.attribute_id = attributeVolume;
    get12.value = read12.data();
    get12.value_size = sizeof(read12);
    Require(sceAudioOut2LoPortGetAttributes(port12, &get12, 1) == 0);
    for (std::size_t i = 0; i < 12; ++i) {
        Require(read12[i] == vol12[i]);
    }
    Require(sceAudioOut2PortDestroy(port12) == 0);
}

void TestPortStateAndDestroyLifecycle(AudioOut2ContextHandle ctx) {
    AudioOut2PortParam param{};
    param.data_format = (2 << 8) | 0;
    param.sampling_freq = 48000;

    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(ctx, &param, &port) == 0 && port != 0);

    AudioOut2PortState state{};
    Require(sceAudioOut2PortGetState(port, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2PortGetState(deadPortHandle, &state) == errorInvalidHandle);
    Require(sceAudioOut2PortGetState(port, &state) == 0);
    Require(state.output == 1 && state.num_channels == 2 && state.volume == 127 && state.reroute_counter == 0 && state.flags == 0);

    Require(sceAudioOut2PortDestroy(port) == 0);
    Require(sceAudioOut2PortDestroy(port) == errorInvalidHandle);
    Require(sceAudioOut2PortDestroy(deadPortHandle) == errorInvalidHandle);
    Require(sceAudioOut2PortGetState(port, &state) == errorInvalidHandle);

    float dummy[2] = {1.0f, 1.0f};
    AudioOut2Attribute dummyAttr{};
    dummyAttr.attribute_id = attributeVolume;
    dummyAttr.value = dummy;
    dummyAttr.value_size = sizeof(dummy);
    Require(sceAudioOut2PortSetAttributes(port, &dummyAttr, 1) == errorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(port, &dummyAttr, 1) == errorInvalidHandle);
}

void TestLegacyAudioOutVolumeAndMute() {
    int vol[8] = {32768, 32768};
    Require(sceAudioOutSetVolume(0, 0x3, vol) == errorInvalidPort);
    Require(sceAudioOutSetVolume(-1, 0x3, vol) == errorInvalidPort);
    Require(sceAudioOutSetVolume(33, 0x3, vol) == errorInvalidPort);
    Require(sceAudioOutSetVolume(1, 0x3, nullptr) == errorInvalidPointer);

    const int legHandle = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 0);
    Require(legHandle > 0 && legHandle <= 32);

    Require(sceAudioOutSetVolume(legHandle, 0x3, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(legHandle, 0x3, vol) == 0);

    vol[0] = 0;
    vol[1] = 0;
    Require(sceAudioOutSetVolume(legHandle, 0x3, vol) == 0);

    Require(sceAudioOutSetMixLevelPadSpk(0, 32768) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(33, 32768) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(legHandle, 32769) == errorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(legHandle, 65536) == errorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(legHandle, 32768) == errorInvalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(legHandle, 0) == errorInvalidPortType);

    Require(sceAudioOutClose(legHandle) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(legHandle, 32768) == errorInvalidPort);
    Require(sceAudioOutSetVolume(legHandle, 0x3, vol) == errorInvalidPort);
}

} // namespace

int main() {
    const int initResult = sceAudioOutInit();
    Require(initResult == 0 || initResult == static_cast<int>(0x8026000e));

    const AudioOut2ContextHandle ctx = InitAndCreateContext();
    TestPortFormatConfigurations(ctx);
    TestVolumeBoundsAndMuting(ctx);
    TestPortStateAndDestroyLifecycle(ctx);
    TestLegacyAudioOutVolumeAndMute();

    Require(sceAudioOut2ContextDestroy(ctx) == 0);
    Require(sceAudioOut2Terminate() == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
