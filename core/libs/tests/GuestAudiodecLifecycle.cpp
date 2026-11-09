#include "prx/libSceAudiodec/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr std::uint32_t Mp3 = 2;
constexpr std::uint32_t M4aac = 3;
constexpr std::uint32_t ConfigAdts = 1;
constexpr std::uint32_t ConfigRaw = 2;
constexpr std::int32_t ErrorApiFail = static_cast<std::int32_t>(0x807F0000);
constexpr std::int32_t ErrorArg = static_cast<std::int32_t>(0x807F0002);
constexpr std::int32_t ErrorInvalidHandle = static_cast<std::int32_t>(0x807F000F);
constexpr std::int32_t ErrorM4aacInvalidSamplingFreq = static_cast<std::int32_t>(0x807F0300);
constexpr std::int32_t Slots = 64;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Audiodec lifecycle check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

std::int32_t CreateMp3() {
    AudiodecParamMp3 param{sizeof(param), 1};
    AudiodecMp3Info info{};
    info.ui_size = sizeof(info);
    AudiodecCtrl ctrl{&param, &info, nullptr, nullptr};
    return sceAudiodecCreateDecoder(&ctrl, Mp3);
}

std::int32_t CreateAac(std::uint32_t configNumber, std::uint32_t samplingFreqIndex, AudiodecM4aacInfo& info) {
    AudiodecParamM4aac param{sizeof(param), 1, configNumber, samplingFreqIndex, 2, 0};
    info = {sizeof(info), 0, 0, 0, 0};
    AudiodecCtrl ctrl{&param, &info, nullptr, nullptr};
    return sceAudiodecCreateDecoder(&ctrl, M4aac);
}

}

int main() {
    Require(sceAudiodecTermLibrary(Mp3) == 0);
    Require(CreateMp3() == ErrorArg);
    Require(sceAudiodecInitLibrary(Mp3) == 0);

    for (const std::int32_t handle : {0, -1, Slots + 1}) Require(sceAudiodecDeleteDecoder(handle) == ErrorInvalidHandle);
    for (std::int32_t slot = 1; slot <= Slots; ++slot) Require(CreateMp3() == slot);
    Require(CreateMp3() == ErrorApiFail);
    Require(sceAudiodecDeleteDecoder(Slots) == 0);
    Require(sceAudiodecDeleteDecoder(Slots) == ErrorInvalidHandle);
    Require(CreateMp3() == Slots);
    for (std::int32_t slot = 1; slot <= Slots; ++slot) Require(sceAudiodecDeleteDecoder(slot) == 0);

    Require(sceAudiodecInitLibrary(M4aac) == 0);
    AudiodecM4aacInfo info{};
    Require(CreateAac(ConfigRaw, 13, info) == ErrorM4aacInvalidSamplingFreq);
    const std::int32_t adts = CreateAac(ConfigAdts, 13, info);
    Require(adts > 0);
    Require(sceAudiodecDeleteDecoder(adts) == 0);
    const std::int32_t raw = CreateAac(ConfigRaw, 12, info);
    Require(raw > 0 && info.ui_sampling_freq == 7350);
    Require(sceAudiodecDeleteDecoder(raw) == 0);
    Require(sceAudiodecTermLibrary(M4aac) == 0);
    Require(sceAudiodecTermLibrary(Mp3) == 0);
    std::puts("Audiodec lifecycle tests passed");
    return 0;
}
