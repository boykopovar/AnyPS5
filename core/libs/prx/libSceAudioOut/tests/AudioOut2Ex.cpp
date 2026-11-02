#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SceTypes.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) {
    if (!value) std::abort();
}

template <typename TFunction>
static bool RejectsArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80260502);
constexpr int ErrorInvalidHandle = static_cast<int>(0x80260503);
constexpr std::uint32_t FormatStereoFloat = 2u << 8;
constexpr std::uint32_t Frequency = 48000;

void TestUserCreateEx() {
    Require(sceAudioOut2UserCreateEx(0, nullptr, nullptr) == ErrorInvalidArgument);
    AudioOut2UserHandle handle = 0;
    Require(sceAudioOut2UserCreateEx(0, nullptr, &handle) == 0);
    Require(handle != 0);
    Require(sceAudioOut2UserDestroy(handle) == 0);
}

void TestPortCreateAndAttributesEx() {
    AudioOut2ContextParam ctxParams{};
    Require(sceAudioOut2ContextResetParam(&ctxParams) == 0);
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&ctxParams, nullptr, 0, &context) == 0);
    Require(context != 0);

    AudioOut2PortParamEx portParams{};
    portParams.base.port_type = 0;
    portParams.base.data_format = FormatStereoFloat;
    portParams.base.sampling_freq = Frequency;

    Require(sceAudioOut2PortCreateEx(0, &portParams, nullptr) == ErrorInvalidHandle);
    Require(sceAudioOut2PortCreateEx(context, nullptr, nullptr) == ErrorInvalidArgument);

    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreateEx(context, &portParams, &port) == 0);
    Require(port != 0);

    Require(sceAudioOut2PortSetAttributesEx(0, nullptr, 1, 0) == ErrorInvalidHandle);
    Require(sceAudioOut2PortSetAttributesEx(port, nullptr, 1, 0) == ErrorInvalidArgument);

    float pcm[512] = {};
    const void* pcmPtr = pcm;
    const AudioOut2Attribute attr{0, 0, &pcmPtr, sizeof(pcmPtr)};
    Require(sceAudioOut2PortSetAttributesEx(port, &attr, 1, 0) == 0);

    Require(sceAudioOut2PortGetAttributes(0, nullptr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2PortGetAttributes(port, nullptr, 1) == ErrorInvalidArgument);

    const void* readPcmPtr = nullptr;
    AudioOut2Attribute readAttr{0, 0, &readPcmPtr, sizeof(readPcmPtr)};
    Require(sceAudioOut2PortGetAttributes(port, &readAttr, 1) == 0);
    Require(readPcmPtr == pcmPtr);

    Require(sceAudioOut2PortDestroy(port) == 0);
    Require(sceAudioOut2ContextDestroy(context) == 0);
}

void TestLoContextAndLoPortEx() {
    AudioOut2ContextParamEx ctxParams{};
    size_t memorySize = 0;
    Require(sceAudioOut2LoContextQueryMemoryEx(nullptr, &memorySize) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextQueryMemoryEx(&ctxParams, nullptr) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextQueryMemoryEx(&ctxParams, &memorySize) == 0);
    Require(memorySize > 0);

    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2LoContextCreateEx(nullptr, nullptr, 0, &context) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextCreateEx(&ctxParams, nullptr, 0, nullptr) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextCreateEx(&ctxParams, nullptr, 0, &context) == 0);
    Require(context != 0);

    Require(sceAudioOut2LoContextGetAttributes(0, nullptr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextGetAttributes(context, nullptr, 1) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextGetAttributes(context, nullptr, 0) == 0);

    AudioOut2PortParamEx portParams{};
    portParams.base.port_type = 0;
    portParams.base.data_format = FormatStereoFloat;
    portParams.base.sampling_freq = Frequency;

    Require(sceAudioOut2LoPortCreateEx(0, &portParams, nullptr) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortCreateEx(context, nullptr, nullptr) == ErrorInvalidArgument);

    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2LoPortCreateEx(context, &portParams, &port) == 0);
    Require(port != 0);

    Require(sceAudioOut2LoPortSetAttributesEx(0, nullptr, 1, 0) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 1, 0) == ErrorInvalidArgument);

    float volume[2] = {0.8f, 0.8f};
    const AudioOut2Attribute volAttr{1, 0, volume, sizeof(volume)};
    Require(sceAudioOut2LoPortSetAttributesEx(port, &volAttr, 1, 0) == 0);

    Require(sceAudioOut2LoPortGetAttributes(0, nullptr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(port, nullptr, 1) == ErrorInvalidArgument);

    float readVolume[2] = {};
    AudioOut2Attribute readVolAttr{1, 0, readVolume, sizeof(readVolume)};
    Require(sceAudioOut2LoPortGetAttributes(port, &readVolAttr, 1) == 0);
    Require(std::fabs(readVolume[0] - 0.8f) < 1e-4f);
    Require(std::fabs(readVolume[1] - 0.8f) < 1e-4f);

    Require(sceAudioOut2LoPortGetStateEx(0, nullptr) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetStateEx(port, nullptr) == ErrorInvalidArgument);

    AudioOut2PortStateEx portState{};
    Require(sceAudioOut2LoPortGetStateEx(port, &portState) == 0);
    Require(portState.base.num_channels == 2);
    Require(portState.base.output == 1);

    Require(sceAudioOut2LoPortDestroy(port) == 0);
    Require(sceAudioOut2LoContextDestroy(context) == 0);
}

void TestSystemStateExAndTerminate() {
    Require(sceAudioOut2GetSystemStateEx(nullptr) == ErrorInvalidArgument);
    AudioOut2SystemStateEx state{};
    Require(sceAudioOut2GetSystemStateEx(&state) == 0);
    Require(state.base.loudness == 0.0f);

    Require(sceAudioOut2Terminate() == 0);
}

void TestContextBedWrite() {
    Require(sceAudioOut2ContextBedWrite(0, nullptr, 128) == ErrorInvalidHandle);

    AudioOut2ContextParam ctxParams{};
    Require(sceAudioOut2ContextResetParam(&ctxParams) == 0);
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&ctxParams, nullptr, 0, &context) == 0);

    Require(sceAudioOut2ContextBedWrite(context, nullptr, 128) == ErrorInvalidArgument);
    float bedSamples[256] = {};
    Require(sceAudioOut2ContextBedWrite(context, bedSamples, sizeof(bedSamples)) == 0);

    Require(sceAudioOut2ContextDestroy(context) == 0);
}

void TestAmbisonicsEncodeCoefficients() {
    const AudioOut2Position dir{1.0f, 0.0f, 0.0f};
    float coeffs[16] = {};

    Require(RejectsArgument([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(0, nullptr, coeffs, 1); }));
    Require(RejectsArgument([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(0, &dir, nullptr, 1); }));
    Require(RejectsArgument([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(6, &dir, coeffs, 49); }));
    Require(RejectsArgument([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(0, &dir, coeffs, 2); }));

    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(0, &dir, coeffs, 1) == 0);
    Require(std::fabs(coeffs[0] - 1.0f) < 1e-4f);

    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(1, &dir, coeffs, 4) == 0);
    Require(std::fabs(coeffs[0] - 1.0f) < 1e-4f);
}

}

int main() {
    TestUserCreateEx();
    TestPortCreateAndAttributesEx();
    TestLoContextAndLoPortEx();
    TestSystemStateExAndTerminate();
    TestContextBedWrite();
    TestAmbisonicsEncodeCoefficients();
    return 0;
}
