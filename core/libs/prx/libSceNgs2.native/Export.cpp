#include "prx/libSceNgs2.native/include/Api.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"
#include "prx/libSceNgs2.native/src/Ngs2Internal.hpp"
#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {

struct Ngs2CommandRecord {
    std::uint32_t id;
    std::uint8_t flags;
    std::uint8_t type;
    std::uint16_t count;
    union {
        std::uint64_t u64;
        std::uint32_t u32;
        std::int32_t i32;
        float f;
        const void* ptr;
    } value;
};
static_assert(sizeof(Ngs2CommandRecord) == 16);

struct Ngs2StreamOption {
    std::uint64_t size;
    char name[64];
    std::uint32_t flags;
    std::uint32_t num_blocks;
    std::uint32_t buffer_size_1;
    std::uint32_t reserved1;
    std::uint32_t buffer_size_2;
    std::uint32_t reserved2;
    std::uint32_t buffer_size_3;
    std::uint32_t reserved3;
    float default_param;
    std::uint32_t reserved4;
    std::uint8_t reserved_tail[24];
};
static_assert(sizeof(Ngs2StreamOption) == 0x88);

constexpr int NGS2_ERROR_INVALID_OUT_ADDRESS = static_cast<int>(0x804a8010u);
constexpr int NGS2_ERROR_INVALID_OUT_SIZE = static_cast<int>(0x804a8011u);
constexpr int NGS2_ERROR_INVALID_OPTION_SIZE = static_cast<int>(0x804a8013u);
constexpr int NGS2_ERROR_INVALID_BUFFER_INFO = static_cast<int>(0x804a8020u);
constexpr int NGS2_ERROR_INVALID_BUFFER_ALLOCATOR = static_cast<int>(0x804a8023u);
constexpr int NGS2_ERROR_INVALID_STREAM_COMMANDS = static_cast<int>(0x804a8030u);
constexpr int NGS2_ERROR_INVALID_STREAM_QUERY_ID = static_cast<int>(0x804a8031u);
constexpr int NGS2_ERROR_INVALID_SAMPLE_RATE = static_cast<int>(0x804a8051u);
constexpr int NGS2_ERROR_INVALID_NUM_BLOCKS = static_cast<int>(0x804a8053u);
constexpr int NGS2_ERROR_FULL_WAVEFORM_BLOCKS = static_cast<int>(0x804a8054u);
constexpr int NGS2_ERROR_NULL_STREAM_BLOCK_PTR = static_cast<int>(0x804a8055u);
constexpr int NGS2_ERROR_INVALID_SYSTEM_HANDLE = static_cast<int>(0x804a8201u);
constexpr int NGS2_ERROR_INVALID_RACK_HANDLE = static_cast<int>(0x804a8202u);
constexpr int NGS2_ERROR_INVALID_STREAM_HANDLE = static_cast<int>(0x804a8204u);
constexpr int NGS2_ERROR_INVALID_RACK_MODULE_INDEX = static_cast<int>(0x804a8256u);
constexpr int NGS2_ERROR_INVALID_RACK_QUERY_ID = static_cast<int>(0x804a8304u);
constexpr int NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES = static_cast<int>(0x804a8310u);
constexpr int NGS2_ERROR_INVALID_PARAM_ID = static_cast<int>(0x804a835au);
constexpr int NGS2_ERROR_INVALID_SYSTEM_QUERY_ID = static_cast<int>(0x804a835bu);
constexpr int NGS2_ERROR_INVALID_STREAM_STATE_COMMAND = static_cast<int>(0x804a8400u);
constexpr int NGS2_ERROR_INVALID_STREAM_COMMAND_ID = static_cast<int>(0x804a8401u);
constexpr int NGS2_ERROR_STREAM_BUFFER_SIZE_3_ZERO = static_cast<int>(0x804a8402u);
constexpr int NGS2_ERROR_STREAM_BUFFER_SIZE_1_ZERO = static_cast<int>(0x804a8403u);
constexpr int NGS2_ERROR_STREAM_BUFFER_SIZE_2_ZERO = static_cast<int>(0x804a8404u);
constexpr int NGS2_ERROR_SYSTEM_INDEX_RANGE = static_cast<int>(0x804a8410u);


bool IsValidSampleRate(std::uint32_t rate) {
    switch (rate) {
    case 11025:
    case 12000:
    case 22050:
    case 24000:
    case 44100:
    case 48000:
    case 88200:
    case 96000:
    case 176400:
    case 192000:
        return true;
    default:
        return false;
    }
}

int CheckStreamOption(const void* option, Ngs2StreamOption& checked) {
    if (option == nullptr) {
        checked = {};
        checked.size = sizeof(Ngs2StreamOption);
        checked.num_blocks = 4;
        checked.buffer_size_1 = 0x100;
        checked.buffer_size_2 = 0x10;
        checked.buffer_size_3 = 0x10000;
        checked.default_param = 0.5f;
        return 0;
    }
    const auto* opt = static_cast<const Ngs2StreamOption*>(option);
    if (opt->num_blocks < 1 || opt->num_blocks > 32) return NGS2_ERROR_INVALID_NUM_BLOCKS;
    if (opt->buffer_size_3 == 0) return NGS2_ERROR_STREAM_BUFFER_SIZE_3_ZERO;
    if (opt->buffer_size_1 == 0) return NGS2_ERROR_STREAM_BUFFER_SIZE_1_ZERO;
    if (opt->buffer_size_2 == 0) return NGS2_ERROR_STREAM_BUFFER_SIZE_2_ZERO;
    if (opt->size != sizeof(Ngs2StreamOption)) return NGS2_ERROR_INVALID_OPTION_SIZE;
    checked = *opt;
    return 0;
}

std::uint64_t ComputeStreamBufferSize(const Ngs2StreamOption& opt) {
    auto align8 = [](std::size_t val) -> std::size_t {
        return (val + 7u) & ~std::size_t{7u};
    };
    const std::size_t guard = (opt.flags & 0x80000000u) ? 16u : 0u;
    const std::size_t n = opt.num_blocks;
    const std::size_t total = align8(0x368 + guard)
                            + align8(0x28 * n + guard)
                            + align8(0x28 * n + guard)
                            + align8(opt.buffer_size_3 + guard)
                            + align8(opt.buffer_size_2 + guard)
                            + align8(opt.buffer_size_1 + guard)
                            + align8(opt.buffer_size_1 + guard)
                            + 8;
    return static_cast<std::uint64_t>(total);
}

}

extern "C" {

int APS5_VABI sceNgs2SystemRunCommands(uintptr_t system_handle, const void* commands, std::size_t count) {
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (count == 0) return 0;
    const auto* records = static_cast<const Ngs2CommandRecord*>(commands);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& rec = records[i];
        const std::uint32_t cmd = rec.id & 0x00FFFFFFu;
        if (cmd == 0xf00000 || cmd == 0xf00001 || cmd == 0xf00002 || cmd == 0xf00006) {
            continue;
        }
        if (cmd == 0xf00003) {
            if (rec.id >= 0x20000000u) return NGS2_ERROR_SYSTEM_INDEX_RANGE;
        } else if (cmd == 0xf00004) {
            if (!IsValidSampleRate(rec.value.u32)) return NGS2_ERROR_INVALID_SAMPLE_RATE;
            if (system->jobsExecuting) throw std::runtime_error("NGS2: changing sample rate from an active job handler is not implemented");
            for (const auto* rack : system->racks) {
                for (const auto& voice : rack->voices) {
                    for (const auto& filter : voice.filters) {
                        if (filter.enabled && rec.value.u32 != system->option.sample_rate) {
                            throw std::runtime_error("NGS2: changing the sample rate under an enabled sampler filter is not implemented");
                        }
                    }
                }
            }
        } else if (cmd == 0xf00005) {
            const std::uint32_t grain = rec.value.u32;
            if (grain < 64 || grain > system->option.max_grain_samples || (grain % 64) != 0) {
                return NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
            }
            if (system->jobsExecuting) throw std::runtime_error("NGS2: changing grain size from an active job handler is not implemented");
        } else {
            return NGS2_ERROR_INVALID_PARAM_ID;
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& rec = records[i];
        const std::uint32_t cmd = rec.id & 0x00FFFFFFu;
        if (cmd == 0xf00000) {
            const Ngs2Unlocked unlocked;
            sceNgs2SystemLock(system_handle);
        } else if (cmd == 0xf00001) {
            sceNgs2SystemUnlock(system_handle);
        } else if (cmd == 0xf00002) {
            sceNgs2SystemSetUserData(system_handle, rec.value.u64);
        } else if (cmd == 0xf00004) {
            sceNgs2SystemSetSampleRate(system_handle, rec.value.u32);
        } else if (cmd == 0xf00005) {
            sceNgs2SystemSetGrainSamples(system_handle, rec.value.u32);
        } else if (cmd == 0xf00006) {
            system->unnamedParam = rec.value.f;
        } else if (cmd == 0xf00003) {
            const std::uint32_t slot = rec.id >> 24;
            if (slot < system->slotData.size()) {
                if (rec.value.ptr == nullptr || *reinterpret_cast<const std::uint64_t*>(rec.value.ptr) == 0) {
                    system->slotSet[slot] = false;
                    std::memset(system->slotData[slot].data(), 0, 24);
                } else {
                    system->slotSet[slot] = true;
                    std::memcpy(system->slotData[slot].data(), rec.value.ptr, 24);
                }
            }
        }
    }
    return 0;
}

int APS5_VABI sceNgs2SystemQueryInfo(uintptr_t system_handle, std::uint32_t info_id, void* info, std::size_t info_size) {
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    switch (info_id & 0x00FFFFFFu) {
    case 0xf04000: {
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        if (info_size < 0xb8) return NGS2_ERROR_INVALID_OUT_SIZE;
        return sceNgs2SystemGetInfo(system_handle, static_cast<Ngs2SystemInfo*>(info), sizeof(Ngs2SystemInfo));
    }
    case 0xf04001: {
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        if (info_size < 8) return NGS2_ERROR_INVALID_OUT_SIZE;
        std::memcpy(info, &system->userData, sizeof(std::uint64_t));
        return 0;
    }
    case 0xf04002: {
        if (info_id >= 0x20000000u) return NGS2_ERROR_SYSTEM_INDEX_RANGE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        if (info_size < 0x18) return NGS2_ERROR_INVALID_OUT_SIZE;
        const std::uint32_t slot = info_id >> 24;
        if (slot < system->slotData.size() && system->slotSet[slot]) {
            std::memcpy(info, system->slotData[slot].data(), 24);
        } else {
            std::memset(info, 0, 24);
        }
        return 0;
    }
    case 0xf04003: {
        if (info == nullptr && info_size >= 8) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        const auto count = static_cast<std::uint32_t>(system->racks.size());
        if (info != nullptr && info_size >= 8) {
            sceNgs2SystemEnumRackHandles(system_handle, static_cast<uintptr_t*>(info), static_cast<std::uint32_t>(info_size / sizeof(uintptr_t)));
        }
        return static_cast<int>(count);
    }
    case 0xf04004: {
        if (info == nullptr && info_size >= 8) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    case 0xf04070: {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    default:
        return NGS2_ERROR_INVALID_SYSTEM_QUERY_ID;
    }
}

int APS5_VABI sceNgs2RackRunCommands(uintptr_t rack_handle, const void* commands, std::size_t count) {
    const Ngs2Lock lock;
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return NGS2_ERROR_INVALID_RACK_HANDLE;
    if (count == 0) return 0;
    const auto* records = static_cast<const Ngs2CommandRecord*>(commands);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t cmd = records[i].id & 0x00FFFFFFu;
        if (cmd < 0xf10000 || cmd > 0xf10002) return NGS2_ERROR_INVALID_PARAM_ID;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& rec = records[i];
        const std::uint32_t cmd = rec.id & 0x00FFFFFFu;
        if (cmd == 0xf10000) {
            sceNgs2RackLock(rack_handle);
        } else if (cmd == 0xf10001) {
            sceNgs2RackUnlock(rack_handle);
        } else if (cmd == 0xf10002) {
            sceNgs2RackSetUserData(rack_handle, rec.value.u64);
        }
    }
    return 0;
}

int APS5_VABI sceNgs2RackQueryInfo(uintptr_t rack_handle, std::uint32_t info_id, void* info, std::size_t info_size) {
    const Ngs2Lock lock;
    auto* rack = Ngs2FindRack(rack_handle);
    if (rack == nullptr) return NGS2_ERROR_INVALID_RACK_HANDLE;
    switch (info_id & 0x00FFFFFFu) {
    case 0xf14000: {
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        if (info_size < 0xe8) return NGS2_ERROR_INVALID_OUT_SIZE;
        return sceNgs2RackGetInfo(rack_handle, static_cast<Ngs2RackInfo*>(info), sizeof(Ngs2RackInfo));
    }
    case 0xf14001: {
        if (info_size < 8) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        std::memcpy(info, &rack->userData, sizeof(std::uint64_t));
        return 0;
    }
    case 0xf14002: {
        if (info_size < 0x20) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
        const std::uint32_t moduleIdx = info_id >> 24;
        if (moduleIdx >= rack->customModules.size()) return NGS2_ERROR_INVALID_RACK_MODULE_INDEX;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    case 0xf14070: {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    default:
        return NGS2_ERROR_INVALID_RACK_QUERY_ID;
    }
}

int APS5_VABI sceNgs2StreamResetOption(void* option) {
    if (option == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    Ngs2StreamOption opt{};
    opt.size = sizeof(Ngs2StreamOption);
    std::memset(opt.name, 0, sizeof(opt.name));
    opt.flags = 0;
    opt.num_blocks = 4;
    opt.buffer_size_1 = 0x100;
    opt.reserved1 = 0;
    opt.buffer_size_2 = 0x10;
    opt.reserved2 = 0;
    opt.buffer_size_3 = 0x10000;
    opt.reserved3 = 0;
    opt.default_param = 0.5f;
    opt.reserved4 = 0;
    std::memset(opt.reserved_tail, 0, sizeof(opt.reserved_tail));
    std::memcpy(option, &opt, sizeof(opt));
    return 0;
}

int APS5_VABI sceNgs2StreamQueryBufferSize(const void* option, void* buffer_info) {
    if (buffer_info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::memset(buffer_info, 0, 0x38);
    Ngs2StreamOption checked{};
    const int error = CheckStreamOption(option, checked);
    if (error != 0) return error;
    const std::uint64_t size = ComputeStreamBufferSize(checked);
    std::memcpy(static_cast<std::uint8_t*>(buffer_info) + 8, &size, sizeof(size));
    return 0;
}

int APS5_VABI sceNgs2StreamCreate(uintptr_t system_handle, const void* option, const void* buffer_info, uintptr_t* handle) {
    if (buffer_info == nullptr) return NGS2_ERROR_INVALID_BUFFER_INFO;
    const auto* info = static_cast<const Ngs2ContextBufferInfo*>(buffer_info);
    if (info->host_buffer == nullptr) return NGS2_ERROR_INVALID_BUFFER_INFO;
    if (handle == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    Ngs2StreamOption checked{};
    const int optError = CheckStreamOption(option, checked);
    if (optError != 0) return optError;
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2StreamCreateWithAllocator(uintptr_t system_handle, const void* option, const void* allocator, uintptr_t* handle) {
    if (allocator == nullptr) return NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    const auto* alloc = static_cast<const Ngs2BufferAllocator*>(allocator);
    if (alloc->alloc_handler == nullptr) return NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    if (handle == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    Ngs2StreamOption checked{};
    const int optError = CheckStreamOption(option, checked);
    if (optError != 0) return optError;
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2StreamDestroy(uintptr_t stream_handle, void* buffer_info) {
    static_cast<void>(buffer_info);
    if (stream_handle == 0) return NGS2_ERROR_INVALID_STREAM_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2StreamRunCommands(uintptr_t stream_handle, const void* commands, std::size_t count) {
    if (stream_handle == 0) return NGS2_ERROR_INVALID_STREAM_HANDLE;
    if (commands == nullptr && count != 0) return NGS2_ERROR_INVALID_STREAM_COMMANDS;
    if (count == 0) return 0;
    const auto* records = static_cast<const Ngs2CommandRecord*>(commands);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& rec = records[i];
        const std::uint32_t cmd = rec.id & 0x00FFFFFFu;
        if (cmd == 0xf20000 || cmd == 0xf20004) {
            continue;
        }
        if (cmd == 0xf20001) {
            const auto* ptr = static_cast<const void* const*>(rec.value.ptr);
            if (ptr == nullptr || ptr[0] == nullptr) return NGS2_ERROR_INVALID_STREAM_COMMANDS;
            if (ptr[2] == nullptr) return static_cast<int>(0x804a8203u);
        } else if (cmd == 0xf20002) {
            const std::uint32_t val = rec.value.u32;
            if (val != 1 && val != 2 && val != 16 && val != 32) return NGS2_ERROR_INVALID_STREAM_STATE_COMMAND;
        } else if (cmd == 0xf20003) {
            if (rec.value.ptr == nullptr) return NGS2_ERROR_NULL_STREAM_BLOCK_PTR;
        } else {
            return NGS2_ERROR_INVALID_STREAM_COMMAND_ID;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2StreamQueryInfo(uintptr_t stream_handle, std::uint32_t info_id, void* info, std::size_t info_size) {
    if (stream_handle == 0) return NGS2_ERROR_INVALID_STREAM_HANDLE;
    const std::uint32_t cmd = info_id & 0x00FFFFFFu;
    if (cmd == 0xf24001) {
        if (info_size < 8) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    } else if (cmd == 0xf24002) {
        if (info_size != 0x30) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    } else if (cmd == 0xf24003) {
        if (info_size < 4) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    } else if (cmd == 0xf24004) {
        if (info_size < 8) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    } else if (cmd == 0xf24005) {
        if (info_size < 0xe8) return NGS2_ERROR_INVALID_OUT_SIZE;
        if (info == nullptr) return NGS2_ERROR_INVALID_OUT_ADDRESS;
    } else {
        return NGS2_ERROR_INVALID_STREAM_QUERY_ID;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2ModuleEnumConfigs(void** out, std::uint32_t max) {
    static_cast<void>(out);
    static_cast<void>(max);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2ModuleArrayEnumItems(const void* array, void* out, std::uint32_t max) {
    if (array == nullptr) return 0;
    const auto* bytes = static_cast<const std::uint8_t*>(array);
    std::uint32_t itemSize = 0;
    std::memcpy(&itemSize, bytes + 0xc, sizeof(itemSize));
    const std::uint8_t flag = bytes[8];
    std::uint32_t count = 0;
    std::memcpy(&count, bytes + 0x14 + 4 * flag, sizeof(count));
    const std::uint32_t toCopy = std::min(count, max);
    if (toCopy > 0 && out != nullptr && itemSize > 0) {
        std::size_t offset = 0x20;
        if (flag == 0) {
            std::uint32_t baseSkip = 0;
            std::memcpy(&baseSkip, bytes + 0x10, sizeof(baseSkip));
            offset += static_cast<std::size_t>(baseSkip) * itemSize;
        }
        std::memcpy(out, bytes + offset, static_cast<std::size_t>(toCopy) * itemSize);
    }
    return static_cast<int>(count);
}

int APS5_VABI sceNgs2ModuleQueueEnumItems(const void* queue, void* out, std::uint32_t max) {
    if (queue == nullptr) return 0;
    static_cast<void>(out);
    static_cast<void>(max);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
