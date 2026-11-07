#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

#ifdef APS5_NGS2_PS4_LIBRARY
static constexpr bool PS4_LIBRARY = true;
#else
static constexpr bool PS4_LIBRARY = false;
#endif
static constexpr std::uint32_t MAX_WAVEFORM_BLOCKS = 32;

union RackOptions {
    Ngs2RackOption common;
    Ngs2SamplerRackOption sampler;
    Ngs2SubmixerRackOption submixer;
    Ngs2MasteringRackOption mastering;
    Ngs2ReverbRackOption reverb;
    Ngs2CustomSamplerRackOption customSampler;
    Ngs2CustomSubmixerRackOption customSubmixer;
    Ngs2CustomMasteringRackOption customMastering;
};

static std::size_t RackOptionSize(std::uint32_t rackId) {
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: return sizeof(Ngs2SamplerRackOption);
        case SCE_NGS2_RACK_ID_SUBMIXER: return sizeof(Ngs2SubmixerRackOption);
        case SCE_NGS2_RACK_ID_MASTERING: return sizeof(Ngs2MasteringRackOption);
        case SCE_NGS2_RACK_ID_REVERB: return sizeof(Ngs2ReverbRackOption);
        case SCE_NGS2_RACK_ID_CUSTOM_SAMPLER: return sizeof(Ngs2CustomSamplerRackOption);
        case SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER: return sizeof(Ngs2CustomSubmixerRackOption);
        case SCE_NGS2_RACK_ID_CUSTOM_MASTERING: return sizeof(Ngs2CustomMasteringRackOption);
        default: throw std::runtime_error("NGS2: rack id " + Ngs2Hex(rackId) + " is not implemented");
    }
}

static RackOptions DefaultRackOption(std::uint32_t rackId) {
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER || rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) {
        throw std::runtime_error("NGS2: default custom rack options are not implemented");
    }
    RackOptions options{};
    auto& common = options.common;
    common.size = RackOptionSize(rackId);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_input_delay_blocks = 1;
    common.max_matrices = 1;
    common.max_ports = 8;
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER:
            common.max_voices = 256;
            common.max_input_delay_blocks = 0;
            options.sampler.max_channel_works = 256;
            options.sampler.max_codec_caches = 32;
            options.sampler.max_waveform_blocks = 4;
            options.sampler.max_envelope_points = 4;
            options.sampler.max_filters = 8;
            options.sampler.max_atrac9_decoders = 256;
            options.sampler.max_atrac9_channel_works = 256;
            options.sampler.num_peak_meter_blocks = 8;
            break;
        case SCE_NGS2_RACK_ID_SUBMIXER:
            options.submixer.max_channels = 8;
            options.submixer.max_envelope_points = 4;
            options.submixer.max_filters = 8;
            options.submixer.max_inputs = 1;
            options.submixer.num_peak_meter_blocks = 8;
            break;
        case SCE_NGS2_RACK_ID_REVERB:
            options.reverb.max_channels = 8;
            options.reverb.reverb_size = 1;
            break;
        default:
            common.max_matrices = 0;
            common.max_ports = 0;
            options.mastering.max_channels = 8;
            options.mastering.num_peak_meter_blocks = 8;
            break;
    }
    return options;
}

static std::uint32_t RackMaxChannels(std::uint32_t rackId, const RackOptions& options) {
    if (rackId == SCE_NGS2_RACK_ID_SUBMIXER) return options.submixer.max_channels;
    if (rackId == SCE_NGS2_RACK_ID_MASTERING) return options.mastering.max_channels;
    if (rackId == SCE_NGS2_RACK_ID_REVERB) return options.reverb.max_channels;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) return options.customSubmixer.max_channels;
    return NGS2_MAX_CHANNELS;
}

static int CheckedRackOption(std::uint32_t rackId, const Ngs2RackOption* option, RackOptions& options) {
    const auto size = RackOptionSize(rackId);
    if (option == nullptr) {
        options = DefaultRackOption(rackId);
        return SCE_NGS2_OK;
    }
    if (option->size != size) throw std::invalid_argument("NGS2: unexpected rack option size " + std::to_string(option->size));
    options = {};
    std::memcpy(&options, option, size);
    if (!PS4_LIBRARY && (rackId == SCE_NGS2_RACK_ID_SAMPLER || rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER)) {
        const auto blocks = rackId == SCE_NGS2_RACK_ID_SAMPLER ? options.sampler.max_waveform_blocks : options.customSampler.max_waveform_blocks;
        if (blocks == 0 || blocks > MAX_WAVEFORM_BLOCKS) return SCE_NGS2_ERROR_INVALID_NUM_WAVEFORM_BLOCKS;
    }
    const auto maxChannels = RackMaxChannels(rackId, options);
    if ((rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) && maxChannels > SCE_NGS2_CUSTOM_SUBMIXER_MAX_CHANNELS) return SCE_NGS2_ERROR_INVALID_NUM_CHANNELS;
    if ((rackId == SCE_NGS2_RACK_ID_SAMPLER && (options.sampler.num_peak_meter_blocks < 1 || options.sampler.num_peak_meter_blocks > 256)) ||
        (rackId == SCE_NGS2_RACK_ID_SUBMIXER && (options.submixer.num_peak_meter_blocks < 1 || options.submixer.num_peak_meter_blocks > 256))) {
        return SCE_NGS2_ERROR_INVALID_PEAK_METER_BLOCKS;
    }
    if ((rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) && (options.customSubmixer.max_inputs == 0 || options.customSubmixer.max_inputs > SCE_NGS2_CUSTOM_SUBMIXER_MAX_INPUTS)) {
        throw std::runtime_error("NGS2: custom submixer racks with " + std::to_string(options.customSubmixer.max_inputs) + " inputs are not implemented");
    }
    if (options.common.max_voices == 0 || maxChannels == 0) throw std::invalid_argument("NGS2: invalid rack voice or channel count");
    int result = SCE_NGS2_OK;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER) result = Ngs2CheckCustomRack(rackId, options.customSampler.custom_rack_option, 0);
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) result = Ngs2CheckCustomRack(rackId, options.customSubmixer.custom_rack_option, options.customSubmixer.max_inputs);
    if (result != SCE_NGS2_OK) return result;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER && options.customSampler.max_waveform_blocks == 0) return SCE_NGS2_ERROR_INVALID_NUM_WAVEFORM_BLOCKS;
    if (maxChannels > NGS2_MAX_CHANNELS && rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) {
        if (rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) {
            throw std::runtime_error("NGS2: custom mastering racks with " + std::to_string(maxChannels) + " channels are not implemented");
        }
        throw std::invalid_argument("NGS2: invalid rack voice or channel count");
    }
    if (rackId == SCE_NGS2_RACK_ID_REVERB && options.reverb.reverb_size != 1) {
        throw std::runtime_error("NGS2: reverb rack size " + std::to_string(options.reverb.reverb_size) + " is not implemented");
    }
    return SCE_NGS2_OK;
}

static int CreateRack(Ngs2Handle systemHandle, std::uint32_t rackId, const RackOptions& options, const Ngs2ContextBufferInfo& bufferInfo,
                      const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    auto* system = Ngs2FindSystem(systemHandle);
    if (system != nullptr && system->jobsExecuting) throw std::runtime_error("NGS2: creating racks from active job handlers is not implemented");
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    auto* rack = new (Ngs2Place(&bufferInfo, sizeof(Ngs2Rack), alignof(Ngs2Rack))) Ngs2Rack{};
    const auto systemUid = system->uid;
    const auto discard = [](Ngs2Rack* value) { std::destroy_at(value); };
    std::unique_ptr<Ngs2Rack, decltype(discard)> pending(rack, discard);
    rack->system = system;
    rack->rackId = rackId;
    rack->maxChannels = RackMaxChannels(rackId, options);
    rack->maxFilters = rackId == SCE_NGS2_RACK_ID_SAMPLER ? options.sampler.max_filters
                      : rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER ? options.customSampler.custom_rack_option.num_modules
                      : (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) ? options.customSubmixer.custom_rack_option.num_modules : 0;
    std::memcpy(rack->name, options.common.name, sizeof(rack->name));
    rack->name[sizeof(rack->name) - 1] = '\0';
    rack->maxGrainSamples = options.common.max_grain_samples;
    rack->maxWaveformBlocks = rackId == SCE_NGS2_RACK_ID_SAMPLER ? options.sampler.max_waveform_blocks
        : rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER ? options.customSampler.max_waveform_blocks : 0;
    if (rackId == SCE_NGS2_RACK_ID_SAMPLER) rack->maxChannelWorks = options.sampler.max_channel_works;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER) rack->maxChannelWorks = options.customSampler.max_channel_works;
    if (rackId == SCE_NGS2_RACK_ID_SUBMIXER) rack->maxInputs = options.submixer.max_inputs;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) rack->maxInputs = options.customSubmixer.max_inputs;
    rack->maxVoiceChannels = options.common.max_voice_channels == 0 ? 8 : options.common.max_voice_channels;
    rack->maxOutputChannels = options.common.max_output_channels == 0 ? 8 : options.common.max_output_channels;
    std::size_t extra = 0;
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER:
            rack->numModules = 5 + options.sampler.max_filters;
            extra = 32;
            break;
        case SCE_NGS2_RACK_ID_SUBMIXER:
            rack->numModules = 5 + options.submixer.max_filters;
            extra = 16;
            break;
        case SCE_NGS2_RACK_ID_MASTERING:
            rack->numModules = 7;
            rack->maxInputs = 1;
            extra = 8;
            break;
        case SCE_NGS2_RACK_ID_REVERB:
            rack->numModules = 1;
            rack->maxInputs = 1;
            extra = 8;
            break;
        default: {
            const auto& custom = rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER ? options.customSampler.custom_rack_option : options.customSubmixer.custom_rack_option;
            rack->numModules = custom.num_modules;
            rack->numBuffers = custom.num_buffers;
            extra = RackOptionSize(rackId) - sizeof(Ngs2RackOption);
            break;
        }
    }
    if (rackId != SCE_NGS2_RACK_ID_SAMPLER && rackId != SCE_NGS2_RACK_ID_CUSTOM_SAMPLER)
        rack->maxChannelWorks = options.common.max_voices * rack->maxChannels;
    const auto* extension = reinterpret_cast<const std::uint8_t*>(&options) + sizeof(Ngs2RackOption);
    rack->infoExtension.assign(extension, extension + extra);
    if (rackId == SCE_NGS2_RACK_ID_MASTERING) std::fill(rack->infoExtension.begin() + 4, rack->infoExtension.end(), std::uint8_t{0});
    if (rackId == SCE_NGS2_RACK_ID_SAMPLER) rack->peakMeterBlocks = options.sampler.num_peak_meter_blocks;
    if (rackId == SCE_NGS2_RACK_ID_SUBMIXER) rack->peakMeterBlocks = options.submixer.num_peak_meter_blocks;
    if (rackId == SCE_NGS2_RACK_ID_MASTERING) rack->peakMeterBlocks = options.mastering.num_peak_meter_blocks;
    rack->bufferInfo = bufferInfo;
    rack->allocator = allocator;
    rack->voices.resize(options.common.max_voices);
    for (auto& voice : rack->voices) {
        voice.rack = rack;
        voice.ports.resize(options.common.max_ports);
        voice.matrices.resize(options.common.max_matrices);
    }
    int setup = SCE_NGS2_OK;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER) setup = Ngs2SetupUserFx(*rack, options.customSampler.custom_rack_option);
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) setup = Ngs2SetupUserFx(*rack, options.customSubmixer.custom_rack_option);
    if (setup < 0) return setup;
    system = Ngs2FindSystem(systemHandle);
    if (system == nullptr || system->uid != systemUid) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    rack->uid = system->nextRackUid++;
    system->racks.push_back(rack);
    pending.release();
    *handle = reinterpret_cast<Ngs2Handle>(rack);
    return SCE_NGS2_OK;
}

int Ngs2DestroyRack(Ngs2Rack& rack, Ngs2ContextBufferInfo* outBufferInfo) {
    if (rack.lockDepth != 0) Ngs2LeaveExclusive(rack.lockDepth);
    rack.destroying = true;
    auto& racks = rack.system != nullptr ? rack.system->racks : Ngs2DetachedRacks();
    std::erase(racks, &rack);
    Ngs2NotifyWaiters();
    for (auto* other : racks) {
        for (auto& voice : other->voices) {
            for (auto& port : voice.ports) {
                if (port.dest != nullptr && port.dest->rack == &rack) port.dest = nullptr;
            }
            Ngs2DetachPendingPatches(voice, rack);
        }
    }
    rack.system = nullptr;
    const auto bufferInfo = rack.bufferInfo;
    const auto allocator = rack.allocator;
    try {
        Ngs2CleanupUserFx(rack);
    } catch (...) {
        std::destroy_at(&rack);
        Ngs2ReleaseBuffer(allocator, bufferInfo, outBufferInfo);
        throw;
    }
    std::destroy_at(&rack);
    return Ngs2ReleaseBuffer(allocator, bufferInfo, outBufferInfo);
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    RackOptions options{};
    const int result = CheckedRackOption(rack_id, option, options);
    if (result == SCE_NGS2_ERROR_INVALID_NUM_WAVEFORM_BLOCKS) return result;
    if (result != SCE_NGS2_OK) throw std::invalid_argument("NGS2: invalid rack option, rejected at creation with " + Ngs2Hex(static_cast<std::uint32_t>(result)));
    *buffer_info = {};
    buffer_info->host_buffer_size = sizeof(Ngs2Rack);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackCreate(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (buffer_info == nullptr) APS5_INVALID_ARG_EX;
    RackOptions options{};
    const int result = CheckedRackOption(rack_id, option, options);
    if (result != SCE_NGS2_OK) return result;
    const Ngs2Lock lock;
    return CreateRack(system_handle, rack_id, options, *buffer_info, {}, handle);
}

int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (allocator == nullptr || allocator->alloc_handler == nullptr || allocator->free_handler == nullptr) APS5_INVALID_ARG_EX;
    RackOptions options{};
    const int checked = CheckedRackOption(rack_id, option, options);
    if (checked != SCE_NGS2_OK) return checked;
    const Ngs2Lock lock;
    if (Ngs2FindSystem(system_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    Ngs2ContextBufferInfo bufferInfo{};
    bufferInfo.host_buffer_size = sizeof(Ngs2Rack);
    bufferInfo.user_data = allocator->user_data;
    const int result = allocator->alloc_handler(&bufferInfo);
    if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the allocator handler failed with " + std::to_string(result));
    int created;
    try {
        created = CreateRack(system_handle, rack_id, options, bufferInfo, *allocator, handle);
    } catch (...) {
        Ngs2ReleaseBuffer(*allocator, bufferInfo, nullptr);
        throw;
    }
    if (created != SCE_NGS2_OK) Ngs2ReleaseBuffer(*allocator, bufferInfo, nullptr);
    return created;
}

int APS5_VABI sceNgs2RackDestroy(uintptr_t rack_handle, Ngs2ContextBufferInfo* buffer_info) {
    const Ngs2Lock lock;
    if (buffer_info != nullptr) *buffer_info = {};
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    while (rack != nullptr && Ngs2ExclusiveElsewhere()) {
        Ngs2WaitOnce();
        rack = Ngs2FindRack(rack_handle);
    }
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (Ngs2ControlActive(*rack)) throw std::runtime_error("NGS2: destroying a rack from its UserFx2 control handler is not implemented");
    if (rack->system != nullptr && Ngs2RenderingHere(*rack->system)) throw std::runtime_error("NGS2: destroying a rack of a system from that system's render (a voice callback) is not implemented");
    while (rack != nullptr && rack->voiceAccesses != 0) {
        Ngs2WaitOnce();
        rack = Ngs2FindRack(rack_handle);
    }
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    return Ngs2DestroyRack(*rack, buffer_info);
}

int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t rack_handle, uint32_t voice_id, uintptr_t* handle) {
    const Ngs2Lock lock;
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (voice_id >= rack->voices.size()) return SCE_NGS2_ERROR_INVALID_VOICE_INDEX;
    *handle = reinterpret_cast<Ngs2Handle>(&rack->voices[voice_id]);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackGetInfo(uintptr_t rack_handle, Ngs2RackInfo* info, size_t info_size) {
    const Ngs2Lock lock;
    const auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size < sizeof(Ngs2RackInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    if (rack->system == nullptr) throw std::runtime_error("NGS2: information for a detached rack is rejected because the reference dereferences its absent system");
    std::memcpy(info->name, rack->name, std::strlen(rack->name) + 1);
    info->rack_handle = rack_handle;
    info->buffer_info = rack->bufferInfo;
    info->owner_system_handle = reinterpret_cast<Ngs2Handle>(rack->system);
    info->type = rack->rackId == SCE_NGS2_RACK_ID_SAMPLER || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER ? 0 :
        rack->rackId == SCE_NGS2_RACK_ID_MASTERING || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING ? 2 : 1;
    info->rack_id = rack->rackId;
    info->uid = rack->uid;
    info->min_grain_samples = MIN_GRAIN_SAMPLES;
    info->max_grain_samples = rack->maxGrainSamples;
    info->max_voices = static_cast<std::uint32_t>(rack->voices.size());
    info->max_channel_works = rack->maxChannelWorks;
    info->max_inputs = rack->maxInputs;
    info->max_matrices = static_cast<std::uint32_t>(rack->voices.front().matrices.size());
    info->max_ports = static_cast<std::uint32_t>(rack->voices.front().ports.size());
    info->max_voice_channels = rack->maxVoiceChannels;
    info->max_output_channels = rack->maxOutputChannels;
    info->num_modules = rack->numModules;
    info->num_buffers = rack->numBuffers;
    info->state_flags = rack->lockDepth == 0 ? 1 : 5;
    info->last_process_ratio = 0;
    info->last_processed_tick = 0;
    info->render_count = rack->renderCount;
    info->active_voice_count = 0;
    info->active_channel_work_count = 0;
    for (const auto& voice : rack->voices) {
        if (!voice.pendingPlay && (voice.stateFlags & SCE_NGS2_VOICE_STATE_FLAG_INUSE) == 0) continue;
        ++info->active_voice_count;
        info->active_channel_work_count += voice.channels;
    }
    auto* tail = reinterpret_cast<std::uint8_t*>(info) + sizeof(Ngs2RackInfo);
    if (rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) {
        if (info_size >= sizeof(Ngs2RackInfo) + rack->infoExtension.size())
            std::memcpy(tail + 1104, rack->infoExtension.data() + 1104, rack->infoExtension.size() - 1104);
        if (info_size >= 1280) {
            const auto ports = rack->voices.front().ports.size();
            if (info_size < 1208 + ports * sizeof(Ngs2CustomRackPortInfo)) throw std::runtime_error("NGS2: the reference custom rack info write exceeds this output size");
            std::memcpy(tail, rack->infoExtension.data(), 8);
            const std::uint64_t modules = rack->numModules;
            std::memcpy(tail + 8, &modules, sizeof(modules));
            for (std::size_t i = 0; i < rack->customModules.size(); ++i) {
                Ngs2CustomRackModuleInfo module{};
                std::memcpy(&module, rack->infoExtension.data() + 16 + i * sizeof(module), sizeof(module));
                module.option = rack->customModuleOptions[i].empty() ? nullptr : reinterpret_cast<const Ngs2CustomModuleOption*>(rack->customModuleOptions[i].data());
                std::memcpy(tail + 16 + i * sizeof(module), &module, sizeof(module));
            }
            std::memcpy(tail + 976, rack->infoExtension.data() + 976, ports * sizeof(Ngs2CustomRackPortInfo));
        }
    } else if (info_size >= sizeof(Ngs2RackInfo) + rack->infoExtension.size())
        std::memcpy(tail, rack->infoExtension.data(), rack->infoExtension.size());
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2CustomRackGetModuleInfo(uintptr_t rack_handle, uint32_t module_index, Ngs2CustomModuleInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const Ngs2Lock lock;
    const auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (module_index >= rack->customModules.size() || info_size < sizeof(Ngs2CustomModuleInfo)) APS5_INVALID_ARG_EX;
    if (info_size != sizeof(Ngs2CustomModuleInfo)) throw std::runtime_error("NGS2: custom module info of " + std::to_string(info_size) + " bytes is not implemented");
    *info = rack->customModules[module_index];
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackSetUserData(uintptr_t rack_handle, uintptr_t user_data) {
    std::lock_guard lock(Ngs2Mutex());
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    rack->userData = user_data;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackGetUserData(uintptr_t rack_handle, uintptr_t* user_data) {
    std::lock_guard lock(Ngs2Mutex());
    const auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (user_data == nullptr) APS5_INVALID_ARG_EX;
    *user_data = rack->userData;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackLock(uintptr_t rack_handle) {
    const Ngs2Lock lock;
    const auto self = std::this_thread::get_id();
    auto* rack = Ngs2FindRack(rack_handle);
    while (rack != nullptr && Ngs2ExclusiveElsewhere()) {
        Ngs2WaitOnce();
        rack = Ngs2FindRack(rack_handle);
    }
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    Ngs2EnterExclusive();
    rack->lockOwner = self;
    rack->lockDepth++;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackUnlock(uintptr_t rack_handle) {
    const Ngs2Lock lock;
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr || rack->lockDepth == 0 || rack->lockOwner != std::this_thread::get_id()) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (--rack->lockDepth == 0) rack->lockOwner = {};
    Ngs2LeaveExclusive();
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
