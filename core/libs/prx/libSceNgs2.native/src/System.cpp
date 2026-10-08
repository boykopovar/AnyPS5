#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"


static std::vector<Ngs2System*>& Systems() {
    static auto* const systems = new std::vector<Ngs2System*>();
    return *systems;
}

std::string Ngs2Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

std::recursive_mutex& Ngs2Mutex() {
    static auto* const mutex = new std::recursive_mutex();
    return *mutex;
}

Ngs2System* Ngs2FindSystem(Ngs2Handle handle) {
    auto& systems = Systems();
    const auto found = std::find(systems.begin(), systems.end(), reinterpret_cast<Ngs2System*>(handle));
    return found == systems.end() ? nullptr : *found;
}

Ngs2Rack* Ngs2FindRack(Ngs2Handle handle) {
    for (auto* system : Systems()) {
        for (auto* rack : system->racks) {
            if (reinterpret_cast<Ngs2Handle>(rack) == handle) return rack;
        }
    }
    return nullptr;
}

Ngs2Voice* Ngs2FindVoice(Ngs2Handle handle) {
    for (auto* system : Systems()) {
        for (auto* rack : system->racks) {
            for (auto& voice : rack->voices) {
                if (reinterpret_cast<Ngs2Handle>(&voice) == handle) return &voice;
            }
        }
    }
    return nullptr;
}

void* Ngs2Place(const Ngs2ContextBufferInfo* bufferInfo, std::size_t size, std::size_t alignment) {
    if (bufferInfo == nullptr || bufferInfo->host_buffer == nullptr || bufferInfo->host_buffer_size < size) {
        throw std::invalid_argument("NGS2: the context buffer is missing or smaller than the queried size");
    }
    if (reinterpret_cast<std::uintptr_t>(bufferInfo->host_buffer) % alignment != 0) {
        throw std::invalid_argument("NGS2: the context buffer is not aligned to " + std::to_string(alignment) + " bytes");
    }
    return bufferInfo->host_buffer;
}

int Ngs2ReleaseBuffer(const Ngs2BufferAllocator& allocator, Ngs2ContextBufferInfo bufferInfo, Ngs2ContextBufferInfo* outBufferInfo) {
    if (outBufferInfo != nullptr) *outBufferInfo = allocator.free_handler == nullptr ? bufferInfo : Ngs2ContextBufferInfo{};
    return allocator.free_handler == nullptr ? SCE_NGS2_OK : allocator.free_handler(&bufferInfo);
}

static Ngs2ContextBufferInfo Allocate(const Ngs2BufferAllocator* allocator, std::size_t size) {
    if (allocator == nullptr || allocator->alloc_handler == nullptr || allocator->free_handler == nullptr) APS5_INVALID_ARG_EX;
    Ngs2ContextBufferInfo bufferInfo{};
    bufferInfo.host_buffer_size = size;
    bufferInfo.user_data = allocator->user_data;
    const int result = allocator->alloc_handler(&bufferInfo);
    if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the allocator handler failed with " + std::to_string(result));
    return bufferInfo;
}

static Ngs2SystemOption DefaultSystemOption() {
    Ngs2SystemOption option{};
    option.size = sizeof(Ngs2SystemOption);
    option.max_grain_samples = 512;
    option.num_grain_samples = 256;
    option.sample_rate = 48000;
    return option;
}

static Ngs2SystemOption CheckedSystemOption(const Ngs2SystemOption* option) {
    if (option == nullptr) return DefaultSystemOption();
    if (option->size != sizeof(Ngs2SystemOption)) throw std::invalid_argument("NGS2: unexpected system option size " + std::to_string(option->size));
    if (option->flags != 0) throw std::runtime_error("NGS2: system option flags are not implemented");
    if (option->sample_rate == 0 || option->max_grain_samples < MIN_GRAIN_SAMPLES || option->num_grain_samples == 0 ||
        option->num_grain_samples > option->max_grain_samples) {
        throw std::invalid_argument("NGS2: invalid system grain or sample rate");
    }
    return *option;
}

static int CreateSystem(const Ngs2SystemOption& option, const Ngs2ContextBufferInfo& bufferInfo, const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    static std::uint32_t nextUid = 1;
    auto* system = new (Ngs2Place(&bufferInfo, sizeof(Ngs2System), alignof(Ngs2System))) Ngs2System{};
    system->option = option;
    system->bufferInfo = bufferInfo;
    system->allocator = allocator;
    system->uid = nextUid++;
    Systems().push_back(system);
    *handle = reinterpret_cast<Ngs2Handle>(system);
    return SCE_NGS2_OK;
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    CheckedSystemOption(option);
    *buffer_info = {};
    buffer_info->host_buffer_size = sizeof(Ngs2System);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto checked = CheckedSystemOption(option);
    if (buffer_info == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(Ngs2Mutex());
    return CreateSystem(checked, *buffer_info, {}, handle);
}

int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto checked = CheckedSystemOption(option);
    const auto bufferInfo = Allocate(allocator, sizeof(Ngs2System));
    std::lock_guard lock(Ngs2Mutex());
    return CreateSystem(checked, bufferInfo, *allocator, handle);
}

int APS5_VABI sceNgs2SystemDestroy(uintptr_t system_handle, Ngs2ContextBufferInfo* buffer_info) {
    std::lock_guard lock(Ngs2Mutex());
    if (buffer_info != nullptr) *buffer_info = {};
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    while (!system->racks.empty()) {
        const int result = Ngs2DestroyRack(*system->racks.back(), nullptr);
        if (result != SCE_NGS2_OK) return result;
    }
    std::erase(Systems(), system);
    const auto bufferInfo = system->bufferInfo;
    const auto allocator = system->allocator;
    std::destroy_at(system);
    return Ngs2ReleaseBuffer(allocator, bufferInfo, buffer_info);
}

int APS5_VABI sceNgs2SystemGetInfo(uintptr_t system_handle, Ngs2SystemInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size != sizeof(Ngs2SystemInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(Ngs2Mutex());
    const auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    *info = {};
    std::memcpy(info->name, system->option.name, sizeof(info->name));
    info->system_handle = system_handle;
    info->buffer_info = system->bufferInfo;
    info->uid = system->uid;
    info->min_grain_samples = MIN_GRAIN_SAMPLES;
    info->max_grain_samples = system->option.max_grain_samples;
    info->state_flags = 1;
    info->rack_count = static_cast<std::uint32_t>(system->racks.size());
    info->render_count = system->renderCount;
    info->sample_rate = system->option.sample_rate;
    info->num_grain_samples = system->option.num_grain_samples;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t system_handle, uint32_t num_samples) {
    std::lock_guard lock(Ngs2Mutex());
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (num_samples == 0 || num_samples > system->option.max_grain_samples) APS5_INVALID_ARG_EX;
    system->option.num_grain_samples = num_samples;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetSampleRate(uintptr_t system_handle, uint32_t sample_rate) {
    std::lock_guard lock(Ngs2Mutex());
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (sample_rate == 0) APS5_INVALID_ARG_EX;
    for (const auto* rack : system->racks) {
        for (const auto& voice : rack->voices) {
            if (voice.reverb && sample_rate != system->option.sample_rate)
                throw std::runtime_error("NGS2: changing the sample rate under a configured reverb is not implemented");
            for (const auto& filter : voice.filters) {
                if (filter.enabled && sample_rate != system->option.sample_rate) {
                    throw std::runtime_error("NGS2: changing the sample rate under an enabled sampler filter is not implemented");
                }
            }
        }
    }
    system->option.sample_rate = sample_rate;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetUserData(uintptr_t system_handle, uintptr_t user_data) {
    std::lock_guard lock(Ngs2Mutex());
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    system->userData = user_data;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemGetUserData(uintptr_t system_handle, uintptr_t* user_data) {
    std::lock_guard lock(Ngs2Mutex());
    const auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (user_data == nullptr) APS5_INVALID_ARG_EX;
    *user_data = system->userData;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemLock(uintptr_t system_handle) {
    Ngs2Mutex().lock();
    if (Ngs2FindSystem(system_handle) != nullptr) return SCE_NGS2_OK;
    Ngs2Mutex().unlock();
    return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
}

int APS5_VABI sceNgs2SystemUnlock(uintptr_t system_handle) {
    std::lock_guard lock(Ngs2Mutex());
    if (Ngs2FindSystem(system_handle) == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    Ngs2Mutex().unlock();
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemRender(uintptr_t system_handle, const Ngs2RenderBufferInfo* buffer_info, uint32_t num_buffer_info) {
    std::lock_guard lock(Ngs2Mutex());
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (buffer_info == nullptr || num_buffer_info == 0) APS5_INVALID_ARG_EX;
    Ngs2RenderSystem(*system, buffer_info, num_buffer_info);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemResetOption(Ngs2SystemOption* option) {
    if (option == nullptr) APS5_INVALID_ARG_EX;
    *option = DefaultSystemOption();
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
