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

union RackOptions {
    Ngs2RackOption common;
    Ngs2SamplerRackOption sampler;
    Ngs2SubmixerRackOption submixer;
    Ngs2MasteringRackOption mastering;
    Ngs2CustomSubmixerRackOption customSubmixer;
};

static std::size_t RackOptionSize(std::uint32_t rackId) {
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: return sizeof(Ngs2SamplerRackOption);
        case SCE_NGS2_RACK_ID_SUBMIXER: return sizeof(Ngs2SubmixerRackOption);
        case SCE_NGS2_RACK_ID_MASTERING: return sizeof(Ngs2MasteringRackOption);
        case SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER: return sizeof(Ngs2CustomSubmixerRackOption);
        default: throw std::runtime_error("NGS2: rack id " + Ngs2Hex(rackId) + " is not implemented");
    }
}

static RackOptions DefaultRackOption(std::uint32_t rackId) {
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) throw std::runtime_error("NGS2: default custom rack options are not implemented");
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
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) return options.customSubmixer.max_channels;
    return NGS2_MAX_CHANNELS;
}

static RackOptions CheckedRackOption(std::uint32_t rackId, const Ngs2RackOption* option) {
    const auto size = RackOptionSize(rackId);
    if (option == nullptr) return DefaultRackOption(rackId);
    if (option->size != size) throw std::invalid_argument("NGS2: unexpected rack option size " + std::to_string(option->size));
    RackOptions options{};
    std::memcpy(&options, option, size);
    const auto maxChannels = RackMaxChannels(rackId, options);
    if (options.common.max_voices == 0 || maxChannels == 0 || maxChannels > NGS2_MAX_CHANNELS) {
        throw std::invalid_argument("NGS2: invalid rack voice or channel count");
    }
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) Ngs2CheckCustomRack(options.customSubmixer.custom_rack_option);
    return options;
}

static int CreateRack(Ngs2Handle systemHandle, std::uint32_t rackId, const RackOptions& options, const Ngs2ContextBufferInfo& bufferInfo,
                      const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    static std::uint32_t nextUid = 1;
    auto* system = Ngs2FindSystem(systemHandle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    auto* rack = new (Ngs2Place(&bufferInfo, sizeof(Ngs2Rack), alignof(Ngs2Rack))) Ngs2Rack{};
    rack->system = system;
    rack->rackId = rackId;
    rack->maxChannels = RackMaxChannels(rackId, options);
    rack->maxFilters = rackId == SCE_NGS2_RACK_ID_SAMPLER ? options.sampler.max_filters : 0;
    rack->uid = nextUid++;
    std::memcpy(rack->name, options.common.name, sizeof(rack->name));
    rack->name[sizeof(rack->name) - 1] = '\0';
    rack->maxGrainSamples = options.common.max_grain_samples;
    if (rackId == SCE_NGS2_RACK_ID_SAMPLER) rack->maxChannelWorks = options.sampler.max_channel_works;
    if (rackId == SCE_NGS2_RACK_ID_SUBMIXER) rack->maxInputs = options.submixer.max_inputs;
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) rack->maxInputs = options.customSubmixer.max_inputs;
    rack->bufferInfo = bufferInfo;
    rack->allocator = allocator;
    rack->voices.resize(options.common.max_voices);
    for (auto& voice : rack->voices) {
        voice.rack = rack;
        voice.ports.resize(options.common.max_ports);
        voice.matrices.resize(options.common.max_matrices);
    }
    if (rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) Ngs2SetupUserFx(*rack, options.customSubmixer.custom_rack_option);
    system->racks.push_back(rack);
    *handle = reinterpret_cast<Ngs2Handle>(rack);
    return SCE_NGS2_OK;
}

int Ngs2DestroyRack(Ngs2Rack& rack, Ngs2ContextBufferInfo* outBufferInfo) {
    Ngs2CleanupUserFx(rack);
    auto& racks = rack.system->racks;
    std::erase(racks, &rack);
    for (auto* other : racks) {
        for (auto& voice : other->voices) {
            for (auto& port : voice.ports) {
                if (port.dest != nullptr && port.dest->rack == &rack) port.dest = nullptr;
            }
        }
    }
    const auto bufferInfo = rack.bufferInfo;
    const auto allocator = rack.allocator;
    std::destroy_at(&rack);
    return Ngs2ReleaseBuffer(allocator, bufferInfo, outBufferInfo);
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    CheckedRackOption(rack_id, option);
    *buffer_info = {};
    buffer_info->host_buffer_size = sizeof(Ngs2Rack);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackCreate(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto options = CheckedRackOption(rack_id, option);
    if (buffer_info == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(Ngs2Mutex());
    return CreateRack(system_handle, rack_id, options, *buffer_info, {}, handle);
}

int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto options = CheckedRackOption(rack_id, option);
    if (allocator == nullptr || allocator->alloc_handler == nullptr || allocator->free_handler == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(Ngs2Mutex());
    if (Ngs2FindSystem(system_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    Ngs2ContextBufferInfo bufferInfo{};
    bufferInfo.host_buffer_size = sizeof(Ngs2Rack);
    bufferInfo.user_data = allocator->user_data;
    const int result = allocator->alloc_handler(&bufferInfo);
    if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the allocator handler failed with " + std::to_string(result));
    return CreateRack(system_handle, rack_id, options, bufferInfo, *allocator, handle);
}

int APS5_VABI sceNgs2RackDestroy(uintptr_t rack_handle, Ngs2ContextBufferInfo* buffer_info) {
    std::lock_guard lock(Ngs2Mutex());
    if (buffer_info != nullptr) *buffer_info = {};
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    return Ngs2DestroyRack(*rack, buffer_info);
}

int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t rack_handle, uint32_t voice_id, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(Ngs2Mutex());
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (voice_id >= rack->voices.size()) APS5_INVALID_ARG_EX;
    *handle = reinterpret_cast<Ngs2Handle>(&rack->voices[voice_id]);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackGetInfo(uintptr_t rack_handle, Ngs2RackInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size != sizeof(Ngs2RackInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(Ngs2Mutex());
    const auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    *info = {};
    std::memcpy(info->name, rack->name, sizeof(info->name));
    info->rack_handle = rack_handle;
    info->buffer_info = rack->bufferInfo;
    info->owner_system_handle = reinterpret_cast<Ngs2Handle>(rack->system);
    info->type = rack->rackId >> 12;
    info->rack_id = rack->rackId;
    info->uid = rack->uid;
    info->min_grain_samples = MIN_GRAIN_SAMPLES;
    info->max_grain_samples = rack->maxGrainSamples;
    info->max_voices = static_cast<std::uint32_t>(rack->voices.size());
    info->max_channel_works = rack->maxChannelWorks;
    info->max_inputs = rack->maxInputs;
    info->max_matrices = static_cast<std::uint32_t>(rack->voices.front().matrices.size());
    info->max_ports = static_cast<std::uint32_t>(rack->voices.front().ports.size());
    info->state_flags = 1;
    info->render_count = static_cast<std::uint64_t>(rack->system->renderCount);
    info->active_voice_count = static_cast<std::uint32_t>(std::count_if(rack->voices.begin(), rack->voices.end(),
                                                                        [](const Ngs2Voice& voice) { return voice.state != Ngs2PlayState::Empty; }));
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2RackLock(uintptr_t rack_handle) {
    Ngs2Mutex().lock();
    if (Ngs2FindRack(rack_handle) != nullptr) return SCE_NGS2_OK;
    Ngs2Mutex().unlock();
    return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
}

int APS5_VABI sceNgs2RackUnlock(uintptr_t rack_handle) {
    std::lock_guard lock(Ngs2Mutex());
    if (Ngs2FindRack(rack_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    Ngs2Mutex().unlock();
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
