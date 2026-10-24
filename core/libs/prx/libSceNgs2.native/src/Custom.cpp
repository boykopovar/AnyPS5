#include <algorithm>
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

static const char* BuiltInModuleName(std::uint32_t moduleId) {
    switch (moduleId) {
        case SCE_NGS2_CUSTOM_MODULE_ID_GAIN: return "Gain";
        case SCE_NGS2_CUSTOM_MODULE_ID_REVERB: return "Reverb";
        case SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT: return "PitchShift";
        case SCE_NGS2_CUSTOM_MODULE_ID_CONVOLUTION: return "Convolution";
        default: return nullptr;
    }
}

static std::uint32_t OptionWord(const Ngs2CustomModuleOption& option) {
    std::uint32_t word = 0;
    std::memcpy(&word, reinterpret_cast<const std::uint8_t*>(&option) + sizeof(option), sizeof(word));
    return word;
}

static int CheckBuiltInModule(const Ngs2CustomRackModuleInfo& info) {
    const auto stateSizeIs = [&info](std::uint32_t size) { return info.state_size == 0 || info.state_size == size; };
    if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_GAIN || info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) {
        if (!stateSizeIs(0)) return SCE_NGS2_ERROR_INVALID_STATE_SIZE;
    } else if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_REVERB) {
        if (!stateSizeIs(0)) return SCE_NGS2_ERROR_INVALID_STATE_SIZE;
        if (info.option != nullptr && info.option->size != SCE_NGS2_CUSTOM_REVERB_OPTION_SIZE) return SCE_NGS2_ERROR_INVALID_OPTION_SIZE;
        if (info.option != nullptr && OptionWord(*info.option) > 1) return SCE_NGS2_ERROR_INVALID_REVERB_OPTION;
    } else if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT) {
        if (!stateSizeIs(SCE_NGS2_CUSTOM_PITCH_SHIFT_STATE_SIZE)) return SCE_NGS2_ERROR_INVALID_STATE_SIZE;
        if (info.option != nullptr && info.option->size == SCE_NGS2_CUSTOM_PITCH_SHIFT_OPTION_SIZE && OptionWord(*info.option) != 0) {
            return SCE_NGS2_ERROR_INVALID_PITCH_SHIFT_OPTION;
        }
    }
    return SCE_NGS2_OK;
}

static void CheckDelayModule(std::uint32_t rackId, const Ngs2CustomRackModuleInfo& info) {
    if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || info.state_size != 0 || info.state_offset != 0) {
        throw std::runtime_error("NGS2: this custom Delay rack or module state is not implemented");
    }
    if (info.option == nullptr || info.option->size != sizeof(Ngs2CustomDelayModuleOption)) {
        throw std::runtime_error("NGS2: this custom Delay module option size is not implemented");
    }
    Ngs2CustomDelayModuleOption delay{};
    std::memcpy(&delay, info.option, sizeof(delay));
    if (delay.type > 1 || delay.max_taps == 0 || delay.max_taps > 8 ||
        !(delay.max_length >= 1.0f && delay.max_length <= 20000.0f) || delay.reserved != 0) {
        throw std::runtime_error("NGS2: these custom Delay module option values are not implemented");
    }
}

int Ngs2CheckCustomRack(std::uint32_t rackId, const Ngs2CustomRackOption& option, std::uint32_t maxInputs) {
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING && option.num_modules != 0) {
        throw std::runtime_error("NGS2: custom mastering racks with modules are not implemented");
    }
    if (option.num_buffers == 0 || option.num_buffers > SCE_NGS2_CUSTOM_MAX_BUFFERS || option.num_buffers < maxInputs) {
        return SCE_NGS2_ERROR_INVALID_NUM_BUFFERS;
    }
    if (option.num_modules > SCE_NGS2_CUSTOM_MAX_MODULES || option.rack_option.max_ports > SCE_NGS2_CUSTOM_MAX_PORTS) APS5_INVALID_ARG_EX;
    for (std::uint32_t m = 0; m < option.num_modules; m++) {
        const auto& info = option.module[m];
        if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) {
            if (info.option == nullptr || info.option->size != sizeof(Ngs2CustomUserFx2ModuleOption)) APS5_INVALID_ARG_EX;
            if (reinterpret_cast<const Ngs2CustomUserFx2ModuleOption*>(info.option)->process_handler == nullptr) APS5_INVALID_ARG_EX;
        } else if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_FILTER) {
            if (info.option != nullptr || info.state_size != 0 || info.state_offset != 0) APS5_INVALID_ARG_EX;
        } else if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) {
            if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::runtime_error("NGS2: the custom UserFx module on a custom sampler rack is not implemented");
            if (info.option != nullptr) throw std::runtime_error("NGS2: a custom UserFx module option is not implemented");
        } else if (info.module_id == SCE_NGS2_CUSTOM_MODULE_ID_DELAY) {
            CheckDelayModule(rackId, info);
        } else if (BuiltInModuleName(info.module_id) == nullptr) {
            throw std::runtime_error("NGS2: custom module id " + Ngs2Hex(info.module_id) + " is not implemented");
        }
        if (info.source_buffer_id >= option.num_buffers || info.extra_buffer_id >= option.num_buffers || info.dest_buffer_id >= option.num_buffers) {
            return SCE_NGS2_ERROR_INVALID_BUFFER_INDEX;
        }
        const int result = CheckBuiltInModule(info);
        if (result != SCE_NGS2_OK) return result;
    }
    if (option.num_buffers != 1) throw std::runtime_error("NGS2: custom racks with " + std::to_string(option.num_buffers) + " buffers are not implemented");
    for (std::uint32_t m = 0; m < option.num_modules; m++) {
        const auto moduleId = option.module[m].module_id;
        if (moduleId == SCE_NGS2_CUSTOM_MODULE_ID_GAIN && option.module[m].option != nullptr) throw std::runtime_error("NGS2: an option for the custom Gain module is not implemented");
        if (moduleId == SCE_NGS2_CUSTOM_MODULE_ID_GAIN && rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) continue;
        if (moduleId == SCE_NGS2_CUSTOM_MODULE_ID_GAIN) throw std::runtime_error("NGS2: the custom Gain module on a custom sampler rack is not implemented");
        const char* name = BuiltInModuleName(moduleId);
        if (name != nullptr) throw std::runtime_error(std::string("NGS2: the custom ") + name + " module is not implemented");
    }
    for (std::uint32_t p = 0; p < option.rack_option.max_ports; p++) {
        if (option.port[p].source_buffer_id != 0) APS5_INVALID_ARG_EX;
    }
    return SCE_NGS2_OK;
}

static Ngs2UserFx2SetupContext UserFxContext(Ngs2Rack& rack, std::size_t module, std::uint32_t voiceIndex) {
    auto& fx = rack.voices[voiceIndex].userFx[module];
    return {rack.userFxCommon[module].data(), fx.param.data(), fx.work.data(), rack.userFx[module].user_data,
            static_cast<std::uint32_t>(rack.voices.size()), voiceIndex, {}};
}

void Ngs2SetupUserFx(Ngs2Rack& rack, const Ngs2CustomRackOption& option) {
    rack.customModuleIds.resize(option.num_modules);
    rack.customModules.resize(option.num_modules);
    rack.userFx.resize(option.num_modules);
    rack.userFxCommon.resize(option.num_modules);
    for (auto& voice : rack.voices) {
        voice.userFx.resize(option.num_modules);
        voice.customUserFx.resize(option.num_modules);
        voice.customGain.assign(option.num_modules, {});
    }
    for (std::uint32_t m = 0; m < option.num_modules; m++) {
        rack.customModuleIds[m] = option.module[m].module_id;
        const auto& info = option.module[m];
        rack.customModules[m] = {info.module_id, info.source_buffer_id, info.extra_buffer_id, info.dest_buffer_id, info.state_offset, info.state_size, 0, 0};
        if (option.module[m].module_id == SCE_NGS2_CUSTOM_MODULE_ID_FILTER) continue;
        if (option.module[m].module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) {
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
        } else if (option.module[m].module_id == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) {
            for (std::uint32_t i = 0; i < rack.voices.size(); i++) {
                rack.voices[i].customUserFx[m].flags = 1;
            }
        }
    }
}

void Ngs2CleanupUserFx(Ngs2Rack& rack) {
    for (std::uint32_t i = 0; i < rack.voices.size(); i++) {
        for (std::size_t m = 0; m < rack.userFx.size(); m++) {
            if (rack.customModuleIds[m] != SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) continue;
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

static void SetCustomUserFxParam(Ngs2Voice& voice, std::uint32_t index, const Ngs2CustomVoiceUserFxParam& param) {
    if (index >= voice.customUserFx.size()) APS5_INVALID_ARG_EX;
    if (index >= voice.rack->customModuleIds.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) {
        APS5_INVALID_ARG_EX;
    }
    auto& fx = voice.customUserFx[index];
    fx.handler = param.handler;
    fx.data = {param.user_data0, param.user_data1, param.user_data2};
    fx.flags = 1;
}

static void SetGainParam(Ngs2Voice& voice, std::uint32_t index, const Ngs2CustomVoiceGainParam& param) {
    if (index >= voice.customGain.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_GAIN) APS5_INVALID_ARG_EX;
    for (const float gain : param.gain) {
        if (!(gain >= -32.0f && gain <= 32.0f)) throw std::runtime_error("NGS2: a custom Gain level outside -32 to 32 is not implemented");
    }
    std::copy(std::begin(param.gain), std::end(param.gain), voice.customGain[index].target.begin());
}

static void ProcessGain(Ngs2Voice& voice, Ngs2VoiceCustomGain& gain, const std::vector<float*>& channels, std::uint32_t grain) {
    if (voice.hasSamples) {
        for (std::uint32_t c = 0; c < voice.channels; c++) {
            const float from = gain.current[c];
            const float step = (gain.target[c] - from) / static_cast<float>(grain);
            for (std::uint32_t i = 0; i < grain; i++) channels[c][i] *= from + step * static_cast<float>(i + 1);
        }
        voice.hasSamples = std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
    }
    gain.current = gain.target;
}

void Ngs2ApplyCustomParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    const auto rackId = voice.rack->rackId;
    if ((param.id & ~0xffu) == SCE_NGS2_CUSTOM_VOICE_PARAM_DELAY) {
        throw std::runtime_error("NGS2: the custom Delay module parameters are not implemented");
    }
    if (param.id == SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP) {
        if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
        SetupCustomSubmixer(voice, ParamAs<Ngs2CustomSubmixerVoiceSetupParam>(param));
        return;
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_FILTER) {
        if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SAMPLER && rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) {
            throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
        }
        const auto index = param.id & 0x1f;
        if (index >= voice.rack->customModuleIds.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_FILTER) APS5_INVALID_ARG_EX;
        const auto& custom = ParamAs<Ngs2CustomVoiceFilterParam>(param);
        Ngs2SamplerVoiceFilterParam filter{};
        filter.header = custom.header;
        filter.index = index;
        filter.location = 1;
        filter.type = custom.type;
        filter.channel_mask = custom.channel_mask;
        if (custom.type == 0) {
            filter = {};
        } else if (custom.type == 1) {
            filter.frequency = custom.param.fcq.frequency;
            filter.q = custom.param.fcq.q;
            filter.level = custom.param.fcq.level;
        } else {
            throw std::runtime_error("NGS2: custom filter type " + std::to_string(custom.type) + " is not implemented");
        }
        Ngs2SetFilter(voice, filter);
        return;
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN) {
        if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
        SetGainParam(voice, param.id & 0x1f, ParamAs<Ngs2CustomVoiceGainParam>(param));
        return;
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2) {
        if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SAMPLER && rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) {
            throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
        }
        SetUserFxParam(voice, param.id & 0x1f, ParamAs<Ngs2CustomVoiceUserFx2Param>(param));
        return;
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX) {
        if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
        SetCustomUserFxParam(voice, param.id & 0x1f, ParamAs<Ngs2CustomVoiceUserFxParam>(param));
        return;
    }
    if (rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
    throw std::runtime_error("NGS2: voice param " + Ngs2Hex(param.id) + " is not implemented");
}

void Ngs2ProcessUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate) {
    if (voice.userFx.empty() && voice.customUserFx.empty()) return;
    std::vector<float*> channels(voice.channels);
    for (std::uint32_t c = 0; c < voice.channels; c++) channels[c] = voice.samples.data() + static_cast<std::size_t>(c) * grain;
    for (std::size_t m = 0; m < voice.rack->customModuleIds.size(); m++) {
        if (voice.rack->customModuleIds[m] == SCE_NGS2_CUSTOM_MODULE_ID_DELAY) continue;
        if (voice.rack->customModuleIds[m] == SCE_NGS2_CUSTOM_MODULE_ID_GAIN) {
            ProcessGain(voice, voice.customGain[m], channels, grain);
        } else if (voice.rack->customModuleIds[m] == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) {
            if (!voice.hasSamples) continue;
            auto& fx = voice.userFx[m];
            const auto& module = voice.rack->userFx[m];
            if (module.process_handler == nullptr) continue;
            Ngs2UserFx2ProcessContext context{channels.data(), voice.rack->userFxCommon[m].data(), fx.param.data(), fx.work.data(), fx.state.data(),
                                              module.user_data, fx.flags, voice.channels, voice.channels, grain, sampleRate, 0, {}};
            const int result = module.process_handler(&context);
            if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx2 process handler failed with " + std::to_string(result));
            fx.flags = 0;
        } else if (voice.rack->customModuleIds[m] == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) {
            if (m < voice.customUserFx.size() && voice.customUserFx[m].handler != nullptr) {
                auto& fx = voice.customUserFx[m];
                Ngs2UserFxProcessContext context{channels.data(), fx.data[0], fx.data[1], fx.data[2],
                                                 fx.flags, voice.channels, grain, sampleRate};
                const int result = fx.handler(&context);
                if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx process handler failed with " + std::to_string(result));
                fx.flags = 0;
                voice.hasSamples = std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
            }
        }
    }
}

void Ngs2ProcessLegacyUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate) {
    if (voice.userFxHandler == nullptr) return;
    std::array<float*, NGS2_MAX_CHANNELS> channels{};
    for (std::uint32_t c = 0; c < voice.channels; ++c) channels[c] = voice.samples.data() + static_cast<std::size_t>(c) * grain;
    Ngs2UserFxProcessContext context{channels.data(), voice.userFxData[0], voice.userFxData[1], voice.userFxData[2],
                                    voice.userFxFlags, voice.channels, grain, sampleRate};
    const int result = voice.userFxHandler(&context);
    if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the UserFx process handler failed with " + std::to_string(result));
    voice.userFxFlags = 0;
    voice.hasSamples = std::any_of(voice.samples.begin(), voice.samples.end(), [](float sample) { return sample != 0.0f; });
}
