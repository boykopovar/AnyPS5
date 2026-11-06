#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"
#include "Ngs2Mixer.hpp"

namespace {

constexpr int SCE_NGS2_ERROR_FAIL = static_cast<int>(0x804A0001);
constexpr int SCE_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES = static_cast<int>(0x804A0050);
constexpr int SCE_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES = static_cast<int>(0x804A0051);
constexpr int SCE_NGS2_ERROR_INVALID_OUT_ADDRESS = static_cast<int>(0x804A0053);
constexpr int SCE_NGS2_ERROR_INVALID_OUT_SIZE = static_cast<int>(0x804A0054);
constexpr int SCE_NGS2_ERROR_INVALID_OPTION_ADDRESS = static_cast<int>(0x804A0080);
constexpr int SCE_NGS2_ERROR_INVALID_OPTION_SIZE = static_cast<int>(0x804A0081);
constexpr int SCE_NGS2_ERROR_INVALID_MAX_VOICES = static_cast<int>(0x804A0103);
constexpr int SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS = static_cast<int>(0x804A0180);
constexpr int SCE_NGS2_ERROR_INVALID_BUFFER_SIZE = static_cast<int>(0x804A0181);
constexpr int SCE_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR = static_cast<int>(0x804A0182);
constexpr int SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE = static_cast<int>(0x804A0200);
constexpr int SCE_NGS2_ERROR_INVALID_RACK_HANDLE = static_cast<int>(0x804A0201);
constexpr int SCE_NGS2_ERROR_INVALID_VOICE_HANDLE = static_cast<int>(0x804A0202);
constexpr int SCE_NGS2_ERROR_INVALID_RACK_ID = static_cast<int>(0x804A0210);
constexpr int SCE_NGS2_ERROR_INVALID_VOICE_ID = static_cast<int>(0x804A0211);
// Taken from the PS4 NGS2 error table (shadPS4 ngs2_error.h); the PS5 value is unconfirmed.
constexpr int SCE_NGS2_ERROR_INVALID_VOICE_PORT_INDEX = static_cast<int>(0x804A0304);

// sceNgs2VoiceGetPortInfo output. The title passes a 24-byte buffer (RCX=0x18 at the call),
// which matches the PS4 layout: matrix id, volume, delay samples, destination input, voice.
struct Ngs2VoicePortInfo {
    std::int32_t matrix_id;
    float volume;
    std::uint32_t num_delay_samples;
    std::uint32_t dest_input_id;
    Ngs2Handle dest_voice_handle;
};
static_assert(sizeof(Ngs2VoicePortInfo) == 24 && offsetof(Ngs2VoicePortInfo, dest_voice_handle) == 16);

// Pan work and parameters, sized from ANIMAL WELL's frame (eboot 0x17b10: 40 bytes of work right
// below the stack guard, 16-byte params): speaker angles, the unit of a full turn, the count.
constexpr std::uint32_t MaxPanSpeakers = 8;
struct Ngs2PanWorkLayout {
    float speaker_angles[MaxPanSpeakers];
    float unit_angle;
    std::uint32_t num_speakers;
};
static_assert(sizeof(Ngs2PanWorkLayout) == 40);
struct Ngs2PanParamLayout {
    float angle;
    float distance;
    float fbw_level;
    float lfe_level;
};
static_assert(sizeof(Ngs2PanParamLayout) == 16);

constexpr std::uint32_t MinGrainSamples = 64;
constexpr std::uint32_t MaxGrainSamples = 1024;
constexpr std::uint32_t DefaultGrainSamples = 256;
constexpr std::uint32_t DefaultSampleRate = 48000;
constexpr std::uint32_t MaxRackVoices = 512;
constexpr std::uint32_t SystemStateReady = 1;
constexpr std::size_t SystemBufferBase = 4096;
constexpr std::size_t RackBufferBase = 1024;
constexpr std::size_t RackBufferPerVoice = 512;
constexpr std::size_t GrainBufferScale = 16;

struct Rack;

struct Voice {
    Ngs2Handle handle = 0;
    std::uint32_t id = 0;
    ngs2mix::VoiceMix mix;
    Rack* rack = nullptr;
};

struct System;

struct Rack {
    Ngs2Handle handle = 0;
    std::uint32_t rackId = 0;
    std::uint32_t uid = 0;
    Ngs2RackOption option{};
    Ngs2ContextBufferInfo bufferInfo{};
    bool ownsBuffer = false;
    Ngs2BufferAllocator allocator{};
    bool locked = false;
    System* system = nullptr;
    std::vector<std::unique_ptr<Voice>> voices;
};

struct System {
    Ngs2Handle handle = 0;
    Ngs2SystemOption option{};
    Ngs2ContextBufferInfo bufferInfo{};
    bool ownsBuffer = false;
    Ngs2BufferAllocator allocator{};
    std::uint32_t uid = 0;
    std::uint32_t grainSamples = DefaultGrainSamples;
    std::uint32_t sampleRate = DefaultSampleRate;
    std::int64_t renderCount = 0;
    std::vector<std::unique_ptr<Rack>> racks;
};

std::mutex g_lock;
std::map<Ngs2Handle, std::unique_ptr<System>> g_systems;
std::map<Ngs2Handle, Rack*> g_racks;
std::map<Ngs2Handle, Voice*> g_voices;
Ngs2Handle g_nextHandle = 0x4E470001;
std::uint32_t g_nextUid = 1;

// Debug aid: APS5_TRACE_NGS2=1 logs voice parameter chains, command batches and render requests.
bool TraceNgs2() {
    static const bool value = std::getenv("APS5_TRACE_NGS2") != nullptr;
    return value;
}

void traceParamChain(Ngs2Handle voice, const Ngs2VoiceParamHeader* param) {
    const auto* cursor = reinterpret_cast<const std::uint8_t*>(param);
    for (int index = 0; index < 32 && cursor != nullptr; ++index) {
        Ngs2VoiceParamHeader head{};
        std::memcpy(&head, cursor, sizeof(head));
        std::fprintf(stderr, "[ngs2] control voice 0x%llx param %d size 0x%x next %d id 0x%08x:", static_cast<unsigned long long>(voice), index, head.size, head.next, head.id);
        const std::size_t words = head.size >= 8 && head.size <= 0x100 ? (head.size - 8) / 4 : 0;
        for (std::size_t word = 0; word < words; ++word) {
            std::uint32_t value = 0;
            std::memcpy(&value, cursor + 8 + word * 4, sizeof(value));
            std::fprintf(stderr, " %08x", value);
        }
        std::fprintf(stderr, "\n");
        if (head.next == 0 || head.size < 8 || head.size > 0x400) break;
        cursor += head.next;
    }
}

Ngs2Handle nextHandle() {
    return g_nextHandle += 4;
}

System* findSystem(const Ngs2Handle handle) {
    const auto entry = g_systems.find(handle);
    return entry == g_systems.end() ? nullptr : entry->second.get();
}

Rack* findRack(const Ngs2Handle handle) {
    const auto entry = g_racks.find(handle);
    return entry == g_racks.end() ? nullptr : entry->second;
}

Voice* findVoice(const Ngs2Handle handle) {
    const auto entry = g_voices.find(handle);
    return entry == g_voices.end() ? nullptr : entry->second;
}

int validateSystemOption(const Ngs2SystemOption* option, std::uint32_t* outGrain, std::uint32_t* outRate) {
    std::uint32_t grain = DefaultGrainSamples;
    std::uint32_t rate = DefaultSampleRate;
    if (option != nullptr) {
        if (option->size != sizeof(Ngs2SystemOption)) return SCE_NGS2_ERROR_INVALID_OPTION_SIZE;
        if (option->max_grain_samples != 0) {
            if (option->max_grain_samples < MinGrainSamples || option->max_grain_samples > MaxGrainSamples)
                return SCE_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
            grain = option->max_grain_samples;
        }
        if (option->num_grain_samples != 0) {
            if (option->num_grain_samples < MinGrainSamples || option->num_grain_samples > grain)
                return SCE_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
            grain = option->num_grain_samples;
        }
        if (option->sample_rate != 0) rate = option->sample_rate;
    }
    *outGrain = grain;
    *outRate = rate;
    return 0;
}

constexpr std::size_t Ps5RackOptionSize = 200;
constexpr std::size_t Ps5MasteringRackOptionSize = 184;
constexpr std::size_t Ps5RackOptionFlagsOffset = 72;
constexpr std::size_t Ps5RackOptionMaxGrainOffset = 76;
constexpr std::size_t Ps5RackOptionMaxVoicesOffset = 80;

// Voices of a rack created without an option. ANIMAL WELL creates its sampler rack with a null
// option and then takes a fresh sampler voice per sound (eboot 0x17640: RackGetVoiceHandle with
// an incrementing index into a 256-entry handle table), so the sampler default holds 256.
std::uint32_t defaultRackVoices(const std::uint32_t rackId) {
    return rackId == 0x1000 ? 256 : 1;
}

int normalizeRackOption(const std::uint32_t rackId, const Ngs2RackOption* option, Ngs2RackOption* normalized, std::uint32_t* outVoices) {
    std::uint32_t voices = defaultRackVoices(rackId);
    Ngs2RackOption value{};
    if (option != nullptr) {
        std::size_t optionSize = 0;
        std::memcpy(&optionSize, option, sizeof(optionSize));
        if (optionSize == sizeof(Ngs2RackOption)) {
            value = *option;
        } else if (optionSize == Ps5RackOptionSize || optionSize == Ps5MasteringRackOptionSize) {
            // The observed SDK layouts share these fields; their remaining bytes are opaque.
            value.size = sizeof(Ngs2RackOption);
            std::memcpy(value.name, reinterpret_cast<const std::uint8_t*>(option) + 8, sizeof(value.name));
            std::memcpy(&value.flags, reinterpret_cast<const std::uint8_t*>(option) + Ps5RackOptionFlagsOffset, sizeof(value.flags));
            std::memcpy(&value.max_grain_samples, reinterpret_cast<const std::uint8_t*>(option) + Ps5RackOptionMaxGrainOffset, sizeof(value.max_grain_samples));
            std::memcpy(&value.max_voices, reinterpret_cast<const std::uint8_t*>(option) + Ps5RackOptionMaxVoicesOffset, sizeof(value.max_voices));
        } else {
            return SCE_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        if (value.max_voices > MaxRackVoices) return SCE_NGS2_ERROR_INVALID_MAX_VOICES;
        if (value.max_voices != 0) voices = value.max_voices;
        if (value.max_grain_samples > MaxGrainSamples) return SCE_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    if (normalized != nullptr) *normalized = value;
    *outVoices = voices;
    return 0;
}

std::size_t systemBufferSize(const std::uint32_t grainSamples) {
    return SystemBufferBase + static_cast<std::size_t>(grainSamples) * GrainBufferScale;
}

std::size_t rackBufferSize(const std::uint32_t voices) {
    return RackBufferBase + static_cast<std::size_t>(voices) * RackBufferPerVoice;
}

int acquireBuffer(const Ngs2BufferAllocator* allocator, const std::size_t size, Ngs2ContextBufferInfo* out) {
    if (allocator == nullptr || allocator->alloc_handler == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    Ngs2ContextBufferInfo info{};
    info.host_buffer_size = size;
    info.user_data = allocator->user_data;
    if (allocator->alloc_handler(&info) != 0 || info.host_buffer == nullptr) return SCE_NGS2_ERROR_FAIL;
    if (info.host_buffer_size < size) return SCE_NGS2_ERROR_INVALID_BUFFER_SIZE;
    *out = info;
    return 0;
}

void releaseBuffer(const Ngs2BufferAllocator& allocator, Ngs2ContextBufferInfo& info) {
    if (allocator.free_handler != nullptr) allocator.free_handler(&info);
}

int createSystem(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo& bufferInfo, const bool ownsBuffer,
                 const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    std::uint32_t grain = 0;
    std::uint32_t rate = 0;
    const int optionResult = validateSystemOption(option, &grain, &rate);
    if (optionResult != 0) return optionResult;
    if (bufferInfo.host_buffer == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    if (bufferInfo.host_buffer_size < systemBufferSize(grain)) return SCE_NGS2_ERROR_INVALID_BUFFER_SIZE;

    auto created = std::make_unique<System>();
    created->handle = nextHandle();
    if (option != nullptr) created->option = *option;
    created->bufferInfo = bufferInfo;
    created->ownsBuffer = ownsBuffer;
    created->allocator = allocator;
    created->uid = g_nextUid++;
    created->grainSamples = grain;
    created->sampleRate = rate;
    *handle = created->handle;
    g_systems[created->handle] = std::move(created);
    return 0;
}

int createRack(System* system, const std::uint32_t rackId, const Ngs2RackOption* option,
               const Ngs2ContextBufferInfo& bufferInfo, const bool ownsBuffer,
               const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    if (TraceNgs2() && option != nullptr) {
        std::size_t optionSize = 0;
        std::memcpy(&optionSize, option, sizeof(optionSize));
        std::fprintf(stderr, "[ngs2] rack option id 0x%x size %zu:", rackId, optionSize);
        for (std::size_t word = 0; word < optionSize / 4 && word < 64; ++word) {
            std::uint32_t value = 0;
            std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(option) + word * 4, sizeof(value));
            std::fprintf(stderr, " %08x", value);
        }
        std::fputc('\n', stderr);
    }
    std::uint32_t voices = 0;
    Ngs2RackOption normalizedOption{};
    const int optionResult = normalizeRackOption(rackId, option, &normalizedOption, &voices);
    if (optionResult != 0) return optionResult;
    if (bufferInfo.host_buffer == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    if (bufferInfo.host_buffer_size < rackBufferSize(voices)) return SCE_NGS2_ERROR_INVALID_BUFFER_SIZE;

    auto created = std::make_unique<Rack>();
    created->handle = nextHandle();
    created->rackId = rackId;
    created->uid = g_nextUid++;
    created->option = normalizedOption;
    created->bufferInfo = bufferInfo;
    created->ownsBuffer = ownsBuffer;
    created->allocator = allocator;
    created->system = system;
    for (std::uint32_t index = 0; index < voices; ++index) {
        auto voice = std::make_unique<Voice>();
        voice->handle = nextHandle();
        voice->id = index;
        voice->rack = created.get();
        voice->mix.rackId = rackId;
        g_voices[voice->handle] = voice.get();
        created->voices.push_back(std::move(voice));
    }
    if (TraceNgs2()) std::fprintf(stderr, "[ngs2] rack 0x%llx id 0x%x voices %u\n", static_cast<unsigned long long>(created->handle), rackId, voices);
    *handle = created->handle;
    g_racks[created->handle] = created.get();
    system->racks.push_back(std::move(created));
    return 0;
}

void destroyRack(System* system, Rack* rack, Ngs2ContextBufferInfo* bufferInfo) {
    for (const auto& voice : rack->voices) g_voices.erase(voice->handle);
    g_racks.erase(rack->handle);
    if (bufferInfo != nullptr) *bufferInfo = rack->bufferInfo;
    if (rack->ownsBuffer) releaseBuffer(rack->allocator, rack->bufferInfo);
    for (auto entry = system->racks.begin(); entry != system->racks.end(); ++entry) {
        if (entry->get() == rack) {
            system->racks.erase(entry);
            return;
        }
    }
}

}

extern "C" {

int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::uint32_t grain = 0;
    std::uint32_t rate = 0;
    const int optionResult = validateSystemOption(option, &grain, &rate);
    if (optionResult != 0) return optionResult;
    buffer_info->host_buffer = nullptr;
    buffer_info->host_buffer_size = systemBufferSize(grain);
    return 0;
}

int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, Ngs2Handle* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    std::lock_guard lock(g_lock);
    return createSystem(option, *buffer_info, false, Ngs2BufferAllocator{}, handle);
}

int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption* option, const Ngs2BufferAllocator* allocator, Ngs2Handle* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::uint32_t grain = 0;
    std::uint32_t rate = 0;
    const int optionResult = validateSystemOption(option, &grain, &rate);
    if (optionResult != 0) return optionResult;
    Ngs2ContextBufferInfo info{};
    const int bufferResult = acquireBuffer(allocator, systemBufferSize(grain), &info);
    if (bufferResult != 0) return bufferResult;
    std::lock_guard lock(g_lock);
    const int result = createSystem(option, info, true, *allocator, handle);
    if (result != 0) releaseBuffer(*allocator, info);
    return result;
}

int APS5_VABI sceNgs2SystemDestroy(Ngs2Handle system_handle, Ngs2ContextBufferInfo* buffer_info) {
    std::lock_guard lock(g_lock);
    System* system = findSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    while (!system->racks.empty()) destroyRack(system, system->racks.back().get(), nullptr);
    if (buffer_info != nullptr) *buffer_info = system->bufferInfo;
    if (system->ownsBuffer) releaseBuffer(system->allocator, system->bufferInfo);
    g_systems.erase(system_handle);
    return 0;
}

int APS5_VABI sceNgs2SystemGetInfo(Ngs2Handle system_handle, Ngs2SystemInfo* info, std::size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size < sizeof(Ngs2SystemInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(g_lock);
    const System* system = findSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    std::memset(info, 0, sizeof(Ngs2SystemInfo));
    std::memcpy(info->name, system->option.name, sizeof(info->name));
    info->system_handle = system->handle;
    info->buffer_info = system->bufferInfo;
    info->uid = system->uid;
    info->min_grain_samples = MinGrainSamples;
    info->max_grain_samples = MaxGrainSamples;
    info->state_flags = SystemStateReady;
    info->rack_count = static_cast<std::uint32_t>(system->racks.size());
    info->render_count = system->renderCount;
    info->sample_rate = system->sampleRate;
    info->num_grain_samples = system->grainSamples;
    return 0;
}

int APS5_VABI sceNgs2SystemSetGrainSamples(Ngs2Handle system_handle, std::uint32_t num_samples) {
    if (num_samples < MinGrainSamples || num_samples > MaxGrainSamples) return SCE_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
    std::lock_guard lock(g_lock);
    System* system = findSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    system->grainSamples = num_samples;
    return 0;
}

int APS5_VABI sceNgs2SystemRender(Ngs2Handle system_handle, const Ngs2RenderBufferInfo* buffer_info,
                                 std::uint32_t num_buffer_info) {
    if (TraceNgs2() && buffer_info != nullptr) {
        static int traced = 0;
        for (std::uint32_t index = 0; index < num_buffer_info && traced < 16; ++index, ++traced)
            std::fprintf(stderr, "[ngs2] render buffer %u: %p size 0x%zx type 0x%x channels %u\n", index, buffer_info[index].buffer, buffer_info[index].buffer_size, buffer_info[index].waveform_type, buffer_info[index].num_channels);
    }
    std::lock_guard lock(g_lock);
    System* system = findSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    constexpr std::uint32_t MaxRenderBuffers = 16;
    if (buffer_info == nullptr || num_buffer_info == 0 || num_buffer_info > MaxRenderBuffers) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    for (std::uint32_t index = 0; index < num_buffer_info; ++index) {
        if (buffer_info[index].buffer == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
        if (buffer_info[index].buffer_size == 0) return SCE_NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    // The mix of this system's voices. No voice holds a waveform (VoiceControl applies no
    // parameter yet), so every output is silence, all-zero bytes in both S16 and float formats.
    // Before this the call failed and the title sent its uninitialized render buffers to
    // AudioOut: a constant full-scale buzz. Voice playback replaces the fill when it exists.
    for (std::uint32_t index = 0; index < num_buffer_info; ++index) std::memset(buffer_info[index].buffer, 0, buffer_info[index].buffer_size);
    std::vector<ngs2mix::VoiceMix*> voices;
    for (const auto& rack : system->racks)
        for (const auto& voice : rack->voices) voices.push_back(&voice->mix);
    ngs2mix::Render(voices, [](const std::uint64_t handle) -> ngs2mix::VoiceMix* {
        Voice* destination = findVoice(static_cast<Ngs2Handle>(handle));
        return destination == nullptr ? nullptr : &destination->mix;
    }, buffer_info, num_buffer_info, system->sampleRate);
    ++system->renderCount;
    return 0;
}

int APS5_VABI sceNgs2RackQueryBufferSize(std::uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::uint32_t voices = 0;
    const int optionResult = normalizeRackOption(rack_id, option, nullptr, &voices);
    if (optionResult != 0) return optionResult;
    buffer_info->host_buffer = nullptr;
    buffer_info->host_buffer_size = rackBufferSize(voices);
    return 0;
}

int APS5_VABI sceNgs2RackCreate(Ngs2Handle system_handle, std::uint32_t rack_id, const Ngs2RackOption* option,
                                const Ngs2ContextBufferInfo* buffer_info, Ngs2Handle* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    std::lock_guard lock(g_lock);
    System* system = findSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    return createRack(system, rack_id, option, *buffer_info, false, Ngs2BufferAllocator{}, handle);
}

int APS5_VABI sceNgs2RackCreateWithAllocator(Ngs2Handle system_handle, std::uint32_t rack_id, const Ngs2RackOption* option,
                                             const Ngs2BufferAllocator* allocator, Ngs2Handle* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::uint32_t voices = 0;
    const int optionResult = normalizeRackOption(rack_id, option, nullptr, &voices);
    if (optionResult != 0) return optionResult;
    Ngs2ContextBufferInfo info{};
    const int bufferResult = acquireBuffer(allocator, rackBufferSize(voices), &info);
    if (bufferResult != 0) return bufferResult;
    std::lock_guard lock(g_lock);
    System* system = findSystem(system_handle);
    if (system == nullptr) {
        releaseBuffer(*allocator, info);
        return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    const int result = createRack(system, rack_id, option, info, true, *allocator, handle);
    if (result != 0) releaseBuffer(*allocator, info);
    return result;
}

int APS5_VABI sceNgs2RackDestroy(Ngs2Handle rack_handle, Ngs2ContextBufferInfo* buffer_info) {
    std::lock_guard lock(g_lock);
    Rack* rack = findRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    destroyRack(rack->system, rack, buffer_info);
    return 0;
}

int APS5_VABI sceNgs2RackGetInfo(Ngs2Handle rack_handle, Ngs2RackInfo* info, std::size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size != sizeof(Ngs2RackInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(g_lock);
    const Rack* rack = findRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    std::memset(info, 0, sizeof(Ngs2RackInfo));
    std::memcpy(info->name, rack->option.name, sizeof(info->name));
    info->rack_handle = rack->handle;
    info->buffer_info = rack->bufferInfo;
    info->owner_system_handle = rack->system->handle;
    info->rack_id = rack->rackId;
    info->uid = rack->uid;
    info->min_grain_samples = MinGrainSamples;
    info->max_grain_samples = rack->option.max_grain_samples != 0
                                  ? rack->option.max_grain_samples
                                  : (rack->system->option.max_grain_samples != 0
                                         ? rack->system->option.max_grain_samples
                                         : MaxGrainSamples);
    info->max_voices = static_cast<std::uint32_t>(rack->voices.size());
    info->max_matrices = rack->option.max_matrices;
    info->max_ports = rack->option.max_ports;
    return 0;
}

int APS5_VABI sceNgs2RackGetVoiceHandle(Ngs2Handle rack_handle, std::uint32_t voice_id, Ngs2Handle* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(g_lock);
    const Rack* rack = findRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    if (voice_id >= rack->voices.size()) return SCE_NGS2_ERROR_INVALID_VOICE_ID;
    *handle = rack->voices[voice_id]->handle;
    return 0;
}

int APS5_VABI sceNgs2RackLock(Ngs2Handle rack_handle) {
    std::lock_guard lock(g_lock);
    Rack* rack = findRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    rack->locked = true;
    return 0;
}

int APS5_VABI sceNgs2RackUnlock(Ngs2Handle rack_handle) {
    std::lock_guard lock(g_lock);
    Rack* rack = findRack(rack_handle);
    if (rack == nullptr) return SCE_NGS2_ERROR_INVALID_RACK_HANDLE;
    rack->locked = false;
    return 0;
}

int APS5_VABI sceNgs2VoiceControl(Ngs2Handle voice_handle, const Ngs2VoiceParamHeader* param_list) {
    std::lock_guard lock(g_lock);
    if (findVoice(voice_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (TraceNgs2() && param_list != nullptr) {
        std::fprintf(stderr, "[ngs2] control caller %p\n", __builtin_return_address(0));
        traceParamChain(voice_handle, param_list);
    }
    if (param_list == nullptr) return SCE_NGS2_ERROR_INVALID_OPTION_ADDRESS;
    // Every block in the chain is applied; one the mixer does not model fails the call.
    Voice* voice = findVoice(voice_handle);
    bool applied = true;
    const auto* cursor = reinterpret_cast<const std::uint8_t*>(param_list);
    for (int index = 0; index < 64; ++index) {
        Ngs2VoiceParamHeader head{};
        std::memcpy(&head, cursor, sizeof(head));
        if (head.size < sizeof(head)) return SCE_NGS2_ERROR_INVALID_OPTION_SIZE;
        applied &= ngs2mix::ApplyParam(voice->mix, head.id, cursor + sizeof(head), head.size - sizeof(head));
        if (head.next == 0) break;
        cursor += head.next;
    }
    return applied ? 0 : SCE_NGS2_ERROR_FAIL;
}

int APS5_VABI sceNgs2VoiceRunCommands(Ngs2Handle voice_handle, const void* commands, std::uint32_t num_commands) {
    std::lock_guard lock(g_lock);
    if (findVoice(voice_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (num_commands == 0) return 0;
    if (TraceNgs2() && commands != nullptr) for (std::uint32_t c = 0; c < num_commands && c < 16; ++c) {
        const auto* words = static_cast<const std::uint32_t*>(commands) + c * 8;
        std::fprintf(stderr, "[ngs2] caller %p commands voice 0x%llx count %u: %08x %08x %08x %08x %08x %08x %08x %08x\n", __builtin_return_address(0), static_cast<unsigned long long>(voice_handle), num_commands, words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7]);
    }
    constexpr std::uint32_t MaxCommandsPerBatch = 1024;
    if (commands == nullptr) return SCE_NGS2_ERROR_INVALID_OPTION_ADDRESS;
    if (num_commands > MaxCommandsPerBatch) return SCE_NGS2_ERROR_FAIL;
    // 32-byte command records (see Ngs2Mixer.hpp); an unknown type fails the batch.
    Voice* voice = findVoice(voice_handle);
    bool ran = true;
    for (std::uint32_t index = 0; index < num_commands; ++index)
        ran &= ngs2mix::RunCommand(voice->mix, static_cast<const std::uint32_t*>(commands) + index * 8);
    return ran ? 0 : SCE_NGS2_ERROR_FAIL;
}

int APS5_VABI sceNgs2VoiceGetState(Ngs2Handle voice_handle, Ngs2VoiceState* state, std::size_t state_size) {
    if (state == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (state_size < sizeof(Ngs2VoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(g_lock);
    const Voice* voice = findVoice(voice_handle);
    if (voice == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    state->state_flags = ngs2mix::StateFlags(voice->mix);
    return 0;
}

int APS5_VABI sceNgs2VoiceGetStateFlags(Ngs2Handle voice_handle, std::uint32_t* state_flags) {
    if (state_flags == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(g_lock);
    const Voice* voice = findVoice(voice_handle);
    if (voice == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    *state_flags = ngs2mix::StateFlags(voice->mix);
    return 0;
}

// No voice patching exists yet, so every port of a valid voice is unconnected: no matrix, no
// destination. A rack without an explicit port count has one port per voice.
int APS5_VABI sceNgs2VoiceGetPortInfo(Ngs2Handle voice_handle, std::uint32_t port, Ngs2VoicePortInfo* info, std::size_t info_size) {
    std::lock_guard lock(g_lock);
    const Voice* voice = findVoice(voice_handle);
    if (voice == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size < sizeof(Ngs2VoicePortInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    const std::uint32_t ports = voice->rack->option.max_ports != 0 ? voice->rack->option.max_ports : 1;
    if (port >= ports) return SCE_NGS2_ERROR_INVALID_VOICE_PORT_INDEX;
    *info = Ngs2VoicePortInfo{-1, 0.0f, 0, 0, 0};
    return 0;
}

// sceNgs2VoiceQueryInfo (9eic4AmjGVI). ANIMAL WELL's wrapper (eboot 0x18990) passes
// (voice, 0x4001, out, 8) and reads the first u32 as the channel count of the voice's waveform
// (1 or 2) to pick its pan matrix; it ignores the result. Waveforms are not tracked (VoiceControl
// is not implemented), so no info is known: validate, leave the output untouched and fail.
int APS5_VABI sceNgs2VoiceQueryInfo(Ngs2Handle voice_handle, std::uint32_t info_id, void* info, std::size_t info_size) {
    (void)info_id;
    std::lock_guard lock(g_lock);
    if (findVoice(voice_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size == 0) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    return SCE_NGS2_ERROR_FAIL;
}

// sceNgs2PanInit(work, speaker_angles, unit_angle, num_speakers). The title passes no angles, a unit
// of 360 (degrees) and two speakers. Without angles the speakers sit at the usual layout: stereo at
// -30/+30 degrees, mono in front, more speakers spread evenly from -30 degrees.
int APS5_VABI sceNgs2PanInit(Ngs2PanWorkLayout* work, const float* speaker_angles, float unit_angle, std::uint32_t num_speakers) {
    if (work == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (!(unit_angle > 0.0f) || !std::isfinite(unit_angle) || num_speakers == 0 || num_speakers > MaxPanSpeakers) return SCE_NGS2_ERROR_FAIL;
    Ngs2PanWorkLayout value{};
    value.unit_angle = unit_angle;
    value.num_speakers = num_speakers;
    for (std::uint32_t speaker = 0; speaker < num_speakers; ++speaker) {
        float degrees = 0.0f;
        if (speaker_angles != nullptr) degrees = speaker_angles[speaker] * 360.0f / unit_angle;
        else if (num_speakers == 2) degrees = speaker == 0 ? -30.0f : 30.0f;
        else if (num_speakers > 2) degrees = -30.0f + 360.0f * static_cast<float>(speaker) / static_cast<float>(num_speakers);
        value.speaker_angles[speaker] = degrees;
    }
    *work = value;
    return 0;
}

// sceNgs2PanGetVolumeMatrix(work, params, num_params, matrix_format, out): per parameter, one level
// per speaker. A constant-power law between the two speakers around the source angle (0 = front,
// clockwise, in work units); a source beyond the outermost stereo speaker stays on it. The level
// scales with fbw_level; distance and LFE have no speaker of their own here. The pan law is a
// standard choice, not one confirmed against the SDK.
int APS5_VABI sceNgs2PanGetVolumeMatrix(const Ngs2PanWorkLayout* work, const Ngs2PanParamLayout* params, std::uint32_t num_params, std::uint32_t matrix_format, float* out) {
    (void)matrix_format;
    if (work == nullptr || params == nullptr || out == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto speakers = work->num_speakers;
    if (speakers == 0 || speakers > MaxPanSpeakers || !(work->unit_angle > 0.0f)) return SCE_NGS2_ERROR_FAIL;
    const auto wrap = [](float degrees) {
        degrees = std::fmod(degrees, 360.0f);
        if (degrees < -180.0f) degrees += 360.0f;
        if (degrees >= 180.0f) degrees -= 360.0f;
        return degrees;
    };
    for (std::uint32_t index = 0; index < num_params; ++index) {
        const auto& param = params[index];
        float* levels = out + static_cast<std::size_t>(index) * speakers;
        std::fill(levels, levels + speakers, 0.0f);
        const float level = std::isfinite(param.fbw_level) ? param.fbw_level : 0.0f;
        if (speakers == 1) {
            levels[0] = level;
            continue;
        }
        const float angle = wrap(param.angle * 360.0f / work->unit_angle);
        // The pair of speakers (by angle) that brackets the source.
        std::uint32_t order[MaxPanSpeakers];
        for (std::uint32_t speaker = 0; speaker < speakers; ++speaker) order[speaker] = speaker;
        std::sort(order, order + speakers, [&](std::uint32_t a, std::uint32_t b) { return wrap(work->speaker_angles[a]) < wrap(work->speaker_angles[b]); });
        const float first = wrap(work->speaker_angles[order[0]]);
        const float last = wrap(work->speaker_angles[order[speakers - 1]]);
        std::uint32_t left = order[speakers - 1], right = order[0];
        float position = 0.0f;
        if (speakers == 2 && angle <= first) { left = right = order[0]; }
        else if (speakers == 2 && angle >= last) { left = right = order[1]; }
        else {
            bool found = false;
            for (std::uint32_t i = 0; i + 1 < speakers && !found; ++i) {
                const float a = wrap(work->speaker_angles[order[i]]), b = wrap(work->speaker_angles[order[i + 1]]);
                if (angle >= a && angle <= b) {
                    left = order[i];
                    right = order[i + 1];
                    position = b > a ? (angle - a) / (b - a) : 0.0f;
                    found = true;
                }
            }
            if (!found) {
                // Across the back gap, from the last speaker round to the first.
                const float span = first + 360.0f - last;
                const float offset = angle >= last ? angle - last : angle + 360.0f - last;
                position = span > 0.0f ? offset / span : 0.0f;
            }
        }
        if (left == right) {
            levels[left] = level;
        } else {
            constexpr float HalfPi = 1.57079632679f;
            levels[left] = level * std::cos(position * HalfPi);
            levels[right] = level * std::sin(position * HalfPi);
        }
    }
    return 0;
}

}
