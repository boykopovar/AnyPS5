#include "Ngs2Test.hpp"

#include <array>
#include <cstring>
#include <limits>

namespace {

union RackOption {
    Ngs2RackOption common;
    Ngs2SamplerRackOption sampler;
    Ngs2SubmixerRackOption submixer;
    Ngs2MasteringRackOption mastering;
    Ngs2ReverbRackOption reverb;
};

std::size_t OptionSize(std::uint32_t rackId) {
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: return sizeof(Ngs2SamplerRackOption);
        case SCE_NGS2_RACK_ID_SUBMIXER: return sizeof(Ngs2SubmixerRackOption);
        case SCE_NGS2_RACK_ID_MASTERING: return sizeof(Ngs2MasteringRackOption);
        default: return sizeof(Ngs2ReverbRackOption);
    }
}

uintptr_t CreateSingleVoiceRack(uintptr_t system, std::uint32_t rackId) {
    RackOption option{};
    option.common.size = OptionSize(rackId);
    option.common.max_grain_samples = 512;
    option.common.max_voices = 1;
    option.common.max_input_delay_blocks = 1;
    option.common.max_matrices = 1;
    option.common.max_ports = 8;
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER:
            option.sampler.max_channel_works = 1;
            option.sampler.max_waveform_blocks = 1;
            break;
        case SCE_NGS2_RACK_ID_SUBMIXER:
            option.submixer.max_channels = 2;
            option.submixer.max_inputs = 1;
            break;
        case SCE_NGS2_RACK_ID_MASTERING:
            option.mastering.max_channels = 2;
            break;
        default:
            option.reverb.max_channels = 2;
            option.reverb.reverb_size = 1;
            break;
    }
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(rackId, &option.common, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, rackId, &option.common, &info, &rack) == SCE_NGS2_OK && rack != 0);
    return rack;
}

void CheckOutOfRange(uintptr_t rack, std::uint32_t maxVoices) {
    uintptr_t last = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, maxVoices - 1, &last) == SCE_NGS2_OK && last != 0);
    for (const auto index : std::array{maxVoices, maxVoices + 1, std::numeric_limits<std::uint32_t>::max()}) {
        uintptr_t output = 0x5a5a;
        Require(sceNgs2RackGetVoiceHandle(rack, index, &output) == SCE_NGS2_ERROR_INVALID_VOICE_INDEX);
        Require(output == 0x5a5a);
    }
    Require(sceNgs2RackGetVoiceHandle(rack, maxVoices, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    uintptr_t again = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, maxVoices - 1, &again) == SCE_NGS2_OK && again == last);
}

}

int main() {
    const auto system = CreateSystem();
    for (const auto rackId : std::array{SCE_NGS2_RACK_ID_SAMPLER, SCE_NGS2_RACK_ID_SUBMIXER,
                                      SCE_NGS2_RACK_ID_MASTERING, SCE_NGS2_RACK_ID_REVERB}) {
        const auto rack = CreateSingleVoiceRack(system, rackId);
        CheckOutOfRange(rack, 1);
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    }
    const auto sampler = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo info{};
    Require(sceNgs2RackGetInfo(sampler, &info, sizeof(info)) == SCE_NGS2_OK && info.max_voices == 256);
    CheckOutOfRange(sampler, info.max_voices);
    uintptr_t output = 0x5a5a;
    Require(sceNgs2RackGetVoiceHandle(0x1234, 0, &output) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && output == 0x5a5a);
    Require(sceNgs2RackDestroy(sampler, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    std::puts("NGS2 voice index: PASS");
}
