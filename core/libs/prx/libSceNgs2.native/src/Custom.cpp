#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr std::uint32_t USER_FX2_FLAG_FIRST_PROCESS = 1;
static constexpr std::uint32_t USER_FX2_FLAG_PARAM_CHANGED = 2;

void Ngs2CheckCustomRack(const Ngs2CustomRackOption& option) {
    if (option.num_buffers != 1) throw std::runtime_error("NGS2: custom racks with " + std::to_string(option.num_buffers) + " buffers are not implemented");
    if (option.num_modules > SCE_NGS2_CUSTOM_MAX_MODULES || option.rack_option.max_ports > SCE_NGS2_CUSTOM_MAX_PORTS) APS5_INVALID_ARG_EX;
    for (std::uint32_t m = 0; m < option.num_modules; m++) {
        const auto& info = option.module[m];
        if (info.module_id != SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) throw std::runtime_error("NGS2: custom module id " + Ngs2Hex(info.module_id) + " is not implemented");
        if (info.option == nullptr || info.option->size != sizeof(Ngs2CustomUserFx2ModuleOption)) APS5_INVALID_ARG_EX;
        if (info.source_buffer_id != 0 || info.dest_buffer_id != 0) APS5_INVALID_ARG_EX;
        if (reinterpret_cast<const Ngs2CustomUserFx2ModuleOption*>(info.option)->process_handler == nullptr) APS5_INVALID_ARG_EX;
    }
    for (std::uint32_t p = 0; p < option.rack_option.max_ports; p++) {
        if (option.port[p].source_buffer_id != 0) APS5_INVALID_ARG_EX;
    }
}

static Ngs2UserFx2SetupContext UserFxContext(Ngs2Rack& rack, std::size_t module, std::uint32_t voiceIndex) {
    auto& fx = rack.voices[voiceIndex].userFx[module];
    return {rack.userFxCommon[module].data(), fx.param.data(), fx.work.data(), rack.userFx[module].user_data,
            static_cast<std::uint32_t>(rack.voices.size()), voiceIndex, {}};
}

void Ngs2SetupUserFx(Ngs2Rack& rack, const Ngs2CustomRackOption& option) {
    rack.userFx.resize(option.num_modules);
    rack.userFxCommon.resize(option.num_modules);
    for (auto& voice : rack.voices) voice.userFx.resize(option.num_modules);
    for (std::uint32_t m = 0; m < option.num_modules; m++) {
        const auto& module = *reinterpret_cast<const Ngs2CustomUserFx2ModuleOption*>(option.module[m].option);
        rack.userFx[m] = module;
        rack.userFxCommon[m].resize(module.common_size);
        for (std::uint32_t i = 0; i < rack.voices.size(); i++) {
            auto& fx = rack.voices[i].userFx[m];
            fx.param.resize(module.param_size);
            fx.work.resize(module.work_size);
            fx.state.resize(option.module[m].state_size);
            fx.flags = USER_FX2_FLAG_FIRST_PROCESS;
            if (module.setup_handler == nullptr) continue;
            auto context = UserFxContext(rack, m, i);
            const int result = module.setup_handler(&context);
            if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx2 setup handler failed with " + std::to_string(result));
        }
    }
}

void Ngs2CleanupUserFx(Ngs2Rack& rack) {
    for (std::uint32_t i = 0; i < rack.voices.size(); i++) {
        for (std::size_t m = 0; m < rack.userFx.size(); m++) {
            if (rack.userFx[m].cleanup_handler == nullptr) continue;
            auto context = UserFxContext(rack, m, i);
            const int result = rack.userFx[m].cleanup_handler(&context);
            if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx2 cleanup handler failed with " + std::to_string(result));
        }
    }
}

static void SetupCustomSubmixer(Ngs2Voice& voice, const Ngs2CustomSubmixerVoiceSetupParam& setup) {
    if (setup.flags != 0) throw std::runtime_error("NGS2: custom submixer setup flags are not implemented");
    if (setup.num_input_channels != setup.num_output_channels) throw std::runtime_error("NGS2: custom submixer channel conversion is not implemented");
    if (setup.num_input_channels == 0 || setup.num_input_channels > voice.rack->maxChannels) APS5_INVALID_ARG_EX;
    voice.channels = setup.num_input_channels;
}

static void SetUserFxParam(Ngs2Voice& voice, std::uint32_t index, const Ngs2CustomVoiceUserFx2Param& param) {
    if (index >= voice.userFx.size()) APS5_INVALID_ARG_EX;
    const auto& module = voice.rack->userFx[index];
    if (module.control_handler != nullptr) throw std::runtime_error("NGS2: the UserFx2 control handler is not implemented");
    auto& fx = voice.userFx[index];
    if (param.data_size != fx.param.size() || (param.data == nullptr && param.data_size != 0)) APS5_INVALID_ARG_EX;
    if (param.data_size != 0) std::memcpy(fx.param.data(), param.data, param.data_size);
    fx.flags |= USER_FX2_FLAG_PARAM_CHANGED;
}

void Ngs2ApplyCustomParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    if (voice.rack->rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
    if (param.id == SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP) {
        SetupCustomSubmixer(voice, ParamAs<Ngs2CustomSubmixerVoiceSetupParam>(param));
        return;
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2) {
        SetUserFxParam(voice, param.id & 0x1f, ParamAs<Ngs2CustomVoiceUserFx2Param>(param));
        return;
    }
    throw std::runtime_error("NGS2: voice param " + Ngs2Hex(param.id) + " is not implemented");
}

void Ngs2ProcessUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate) {
    if (voice.userFx.empty()) return;
    std::vector<float*> channels(voice.channels);
    for (std::uint32_t c = 0; c < voice.channels; c++) channels[c] = voice.samples.data() + static_cast<std::size_t>(c) * grain;
    for (std::size_t m = 0; m < voice.userFx.size(); m++) {
        auto& fx = voice.userFx[m];
        const auto& module = voice.rack->userFx[m];
        Ngs2UserFx2ProcessContext context{channels.data(), voice.rack->userFxCommon[m].data(), fx.param.data(), fx.work.data(), fx.state.data(),
                                          module.user_data, fx.flags, voice.channels, voice.channels, grain, sampleRate, 0, {}};
        const int result = module.process_handler(&context);
        if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx2 process handler failed with " + std::to_string(result));
        fx.flags = 0;
    }
}
