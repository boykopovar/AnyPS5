#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {

constexpr int audioOut2ErrorInvalidParam = static_cast<int>(0x80268001);
constexpr int audioOut2ErrorInvalidArgument = static_cast<int>(0x80260502);
constexpr int audioOut2ErrorInvalidHandle = static_cast<int>(0x80260503);

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void TestAudioOut2Set3DLatencyValidation() {
    for (const std::uint32_t output : {0u, 1u, 2u}) {
        Require(sceAudioOut2Set3DLatency(0, output) == 0);
    }
    for (const std::uint32_t output : {3u, 4u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioOut2Set3DLatency(0, output) == audioOut2ErrorInvalidParam);
    }

    Require(sceAudioOut2Set3DLatency(0xFF, 0) == audioOut2ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(255, 0) == audioOut2ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFF, 1) == 0);
    Require(sceAudioOut2Set3DLatency(255, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 2) == 0);
    Require(sceAudioOut2Set3DLatency(255, 2) == 0);
    for (const std::uint32_t output : {3u, 4u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioOut2Set3DLatency(0xFF, output) == audioOut2ErrorInvalidParam);
        Require(sceAudioOut2Set3DLatency(255, output) == audioOut2ErrorInvalidParam);
    }

    for (const int badUserId : {-100, -1, 1, 2, 10, 50, 254, 256, 1000, 0x10000000}) {
        for (const std::uint32_t output : {0u, 1u, 2u, 3u, 10u}) {
            Require(sceAudioOut2Set3DLatency(badUserId, output) == audioOut2ErrorInvalidParam);
        }
    }
}

void TestAudioOut2LoPortSetAttributesExAndPortAttributes() {
    AudioOut2ContextParam ctxParams{};
    Require(sceAudioOut2ContextResetParam(&ctxParams) == 0);

    size_t memorySize = 0;
    Require(sceAudioOut2ContextQueryMemory(&ctxParams, &memorySize) == 0);
    Require(memorySize > 0);

    std::vector<std::uint8_t> contextBuffer(memorySize);
    AudioOut2ContextHandle ctx = 0;
    Require(sceAudioOut2ContextCreate(&ctxParams, contextBuffer.data(), memorySize, &ctx) == 0);
    Require(ctx != 0);

    AudioOut2PortParam portParams{};
    portParams.port_type = 0;
    portParams.data_format = 0x200;
    portParams.sampling_freq = 48000;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(ctx, &portParams, &port) == 0);
    Require(port != 0);

    for (const AudioOut2PortHandle badHandle : {0ULL, 999ULL, 0xFFFFFFFFFFFFFFFFULL}) {
        Require(sceAudioOut2LoPortSetAttributesEx(badHandle, nullptr, 0) == audioOut2ErrorInvalidHandle);
        Require(sceAudioOut2PortSetAttributes(badHandle, nullptr, 0) == audioOut2ErrorInvalidHandle);
        Require(sceAudioOut2PortSetAttributesEx(badHandle, nullptr, 0) == audioOut2ErrorInvalidHandle);
    }

    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 1) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2PortSetAttributes(port, nullptr, 1) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2PortSetAttributesEx(port, nullptr, 1) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 4) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2PortSetAttributes(port, nullptr, 4) == audioOut2ErrorInvalidArgument);
    Require(sceAudioOut2PortSetAttributesEx(port, nullptr, 4) == audioOut2ErrorInvalidArgument);

    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 0) == 0);
    Require(sceAudioOut2PortSetAttributes(port, nullptr, 0) == 0);
    Require(sceAudioOut2PortSetAttributesEx(port, nullptr, 0) == 0);

    float volume[2] = {1.0f, 0.5f};
    AudioOut2Attribute attrVol{};
    attrVol.attribute_id = 1;
    attrVol.value = volume;
    attrVol.value_size = sizeof(volume);

    Require(sceAudioOut2LoPortSetAttributesEx(port, &attrVol, 1) == 0);

    float queryVolume[2] = {0.0f, 0.0f};
    AudioOut2Attribute getAttr{};
    getAttr.attribute_id = 1;
    getAttr.value = queryVolume;
    getAttr.value_size = sizeof(queryVolume);
    Require(sceAudioOut2LoPortGetAttributes(port, &getAttr, 1) == 0);
    Require(queryVolume[0] == 1.0f && queryVolume[1] == 0.5f);

    float newVolume[2] = {0.25f, 0.75f};
    attrVol.value = newVolume;
    Require(sceAudioOut2PortSetAttributes(port, &attrVol, 1) == 0);
    Require(sceAudioOut2LoPortGetAttributes(port, &getAttr, 1) == 0);
    Require(queryVolume[0] == 0.25f && queryVolume[1] == 0.75f);

    float exVolume[2] = {0.125f, 0.875f};
    attrVol.value = exVolume;
    Require(sceAudioOut2PortSetAttributesEx(port, &attrVol, 1) == 0);
    Require(sceAudioOut2LoPortGetAttributes(port, &getAttr, 1) == 0);
    Require(queryVolume[0] == 0.125f && queryVolume[1] == 0.875f);

    std::vector<float> pcm(256 * 2, 0.0f);
    void* pcmPtr = pcm.data();
    AudioOut2Attribute attrData{};
    attrData.attribute_id = 0;
    attrData.value = &pcmPtr;
    attrData.value_size = sizeof(pcmPtr);
    Require(sceAudioOut2LoPortSetAttributesEx(port, &attrData, 1) == 0);

    Require(sceAudioOut2PortDestroy(port) == 0);
    Require(sceAudioOut2ContextDestroy(ctx) == 0);

    Require(sceAudioOut2LoPortSetAttributesEx(port, &attrVol, 1) == audioOut2ErrorInvalidHandle);
    Require(sceAudioOut2PortSetAttributes(port, &attrVol, 1) == audioOut2ErrorInvalidHandle);
    Require(sceAudioOut2PortSetAttributesEx(port, &attrVol, 1) == audioOut2ErrorInvalidHandle);
}

} // namespace

int main() {
    Require(sceAudioOut2Initialize() == 0 || sceAudioOut2Initialize() == static_cast<int>(0x80268004));

    TestAudioOut2Set3DLatencyValidation();
    TestAudioOut2LoPortSetAttributesExAndPortAttributes();

    LibcRunShutdown_nid_postfix();
    return 0;
}
