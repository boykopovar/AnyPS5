#include "prx/libSceSdma/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SdmaErrorNotQueueMode = static_cast<int>(0x80990002);
constexpr int SdmaErrorInvalidArgument = static_cast<int>(0x80990005);
constexpr int SdmaErrorBusy = static_cast<int>(0x80990006);
constexpr int SdmaErrorRingFull = static_cast<int>(0x80990064);
constexpr int SdmaErrorFencePending = static_cast<int>(0x80990065);

constexpr std::uint32_t MinimumWorkingBuffer = 0x8000;
constexpr std::uint32_t FenceSlots = 0x1000;
constexpr std::uint32_t RingMargin = 0x10;
constexpr std::uint32_t LinearPacket = 7;
constexpr std::uint32_t FillPacket = 5;
constexpr std::uint32_t WindowPacket = 13;
constexpr std::uint32_t FencePacket = 4;
constexpr std::uint32_t ChunkBytes = 0x400000;

struct SdmaOpenParam {
    std::uint32_t engine;
    std::uint32_t reserved;
    std::uint64_t fenceAddress;
    std::uint64_t writeBackAddress;
    std::uint64_t writeBackExtra;
    std::uint32_t flags;
};
static_assert(offsetof(SdmaOpenParam, fenceAddress) == 8);
static_assert(offsetof(SdmaOpenParam, writeBackAddress) == 0x10);
static_assert(offsetof(SdmaOpenParam, flags) == 0x20);

struct SdmaSlot {
    bool open = false;
    bool queueMode = false;
    std::uint32_t ringDwords = 0;
    std::uint32_t writeIndex = 0;
    std::uint32_t pendingDwords = 0;
    std::uint32_t fenceIndex = 0;
    std::uint64_t* writeBack = nullptr;
    std::vector<std::function<void()>> deferred;
    std::uint64_t fencesEmitted = 0;
    std::uint64_t fencesExecuted = 0;
    std::uint64_t watchedFence = 0;
};

std::mutex g_mutex;
SdmaSlot g_slots[20];

void Emit(SdmaSlot& slot, std::uint32_t dwords, std::function<void()> operation) {
    slot.writeIndex = static_cast<std::uint32_t>((static_cast<std::uint64_t>(slot.writeIndex) + dwords) % slot.ringDwords);
    slot.pendingDwords += dwords;
    slot.deferred.push_back(std::move(operation));
}

std::uint64_t EmitFence(SdmaSlot& slot) {
    const std::uint64_t fence = ++slot.fencesEmitted;
    Emit(slot, FencePacket, [&slot, fence] { slot.fencesExecuted = fence; });
    return fence;
}

void Kick(SdmaSlot& slot) {
    for (auto& operation : slot.deferred) operation();
    slot.deferred.clear();
    slot.pendingDwords = 0;
    if (slot.writeBack) *slot.writeBack = static_cast<std::uint64_t>(slot.writeIndex) * 4;
}

void FlushAll(SdmaSlot& slot) {
    if (slot.writeIndex == 0 && slot.pendingDwords == 0) return;
    EmitFence(slot);
    Kick(slot);
    slot.watchedFence = 0;
    slot.fenceIndex = 0;
}

std::uint32_t RingPending(const SdmaSlot& slot) { return static_cast<std::uint16_t>(slot.pendingDwords); }

bool Fits(const SdmaSlot& slot, std::uint32_t dwords) {
    return static_cast<std::uint64_t>(RingPending(slot)) + dwords + RingMargin <= slot.ringDwords;
}

int Submit(void* handle, std::uint32_t dwords, bool blocking, const std::function<void(SdmaSlot&)>& emit) {
    std::lock_guard lock(g_mutex);
    SdmaSlot& slot = *static_cast<SdmaSlot*>(handle);
    if (!Fits(slot, dwords)) {
        if (!blocking) return SdmaErrorRingFull;
        FlushAll(slot);
        if (!Fits(slot, dwords)) NotImplemented_nid_no_patch("sceSdma: a command larger than the ring");
    }
    emit(slot);
    if (blocking) {
        EmitFence(slot);
        Kick(slot);
        return 0;
    }
    if (!slot.queueMode) Kick(slot);
    return 0;
}

std::uint32_t LinearDwords(std::uint32_t bytes, std::uint32_t packet) {
    return (bytes / ChunkBytes) * packet + packet;
}

void Chunks(SdmaSlot& slot, std::uint32_t bytes, std::uint32_t packet, const std::function<void(std::uint64_t, std::uint32_t)>& operation) {
    std::uint64_t offset = 0;
    for (std::uint32_t chunk = bytes / ChunkBytes; chunk != 0; --chunk, offset += ChunkBytes) {
        Emit(slot, packet, [operation, offset] { operation(offset, ChunkBytes); });
    }
    if (const std::uint32_t rest = bytes % ChunkBytes; rest != 0) {
        Emit(slot, packet, [operation, offset, rest] { operation(offset, rest); });
    }
}

int CopyLinear(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes, bool blocking) {
    return Submit(handle, LinearDwords(bytes, LinearPacket), blocking, [=](SdmaSlot& slot) {
        Chunks(slot, bytes, LinearPacket, [=](std::uint64_t offset, std::uint32_t size) {
            std::memmove(reinterpret_cast<void*>(destination + offset), reinterpret_cast<const void*>(source + offset), size);
        });
    });
}

int ConstantFill(void* handle, std::uint32_t value, std::uint64_t destination, std::uint32_t bytes, std::uint32_t mode, bool blocking) {
    const auto low = static_cast<std::uint8_t>(value);
    const bool uniform = value == low * 0x01010101u;
    if (!uniform && mode == 0) NotImplemented_nid_no_patch("sceSdmaConstantFill: the pattern of the default fill size");
    if (!uniform && ((destination | bytes) & 3) != 0) NotImplemented_nid_no_patch("sceSdmaConstantFill: a 32-bit pattern on an unaligned range");
    return Submit(handle, LinearDwords(bytes, FillPacket), blocking, [=](SdmaSlot& slot) {
        Chunks(slot, bytes, FillPacket, [=](std::uint64_t offset, std::uint32_t size) {
            auto* target = reinterpret_cast<std::uint8_t*>(destination + offset);
            if (uniform) {
                std::memset(target, low, size);
                return;
            }
            for (std::uint32_t index = 0; index < size; index += 4) std::memcpy(target + index, &value, 4);
        });
    });
}

std::uint32_t ElementBytes(std::uint32_t bitsPerElement) {
    switch (bitsPerElement) {
    case 16: return 2;
    case 32: return 4;
    case 64: return 8;
    case 128: return 16;
    default: return 1;
    }
}

int CopyWindow(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bitsPerElement, std::uint32_t width, std::uint32_t height,
               std::uint32_t sourcePitch, std::uint32_t sourceX, std::uint32_t sourceY, std::uint32_t destinationPitch, std::uint32_t destinationX,
               std::uint32_t destinationY, bool blocking) {
    const bool inRange = width - 1 < 0x4000 && height - 1 < 0x4000 && sourceX < 0x4000 && sourceY < 0x4000 && destinationX < 0x4000 &&
                         destinationY < 0x4000 && sourcePitch - 1 < 0x80000 && destinationPitch - 1 < 0x80000;
    if (!inRange) NotImplemented_nid_no_patch("sceSdmaCopyWindowL2L: a window outside the packet field ranges");
    const std::uint64_t element = ElementBytes(bitsPerElement);
    return Submit(handle, WindowPacket, blocking, [=](SdmaSlot& slot) {
        Emit(slot, WindowPacket, [=] {
            for (std::uint64_t row = 0; row < height; ++row) {
                const std::uint64_t from = source + ((sourceY + row) * sourcePitch + sourceX) * element;
                const std::uint64_t to = destination + ((destinationY + row) * destinationPitch + destinationX) * element;
                std::memmove(reinterpret_cast<void*>(to), reinterpret_cast<const void*>(from), width * element);
            }
        });
    });
}

}

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceSdmaBeginQueueMode(void* handle) {
    std::lock_guard lock(g_mutex);
    auto& slot = *static_cast<SdmaSlot*>(handle);
    FlushAll(slot);
    slot.queueMode = true;
    return 0;
}

int APS5_VABI sceSdmaCancelQueue(void* handle) {
    std::lock_guard lock(g_mutex);
    auto& slot = *static_cast<SdmaSlot*>(handle);
    if (!slot.queueMode) return SdmaErrorNotQueueMode;
    if (slot.pendingDwords != RingPending(slot)) NotImplemented_nid_no_patch("sceSdmaCancelQueue: a queue of more than 65535 dwords, which the module rewinds only in part");
    slot.writeIndex = static_cast<std::uint32_t>((static_cast<std::uint64_t>(slot.writeIndex) + slot.ringDwords - slot.pendingDwords % slot.ringDwords) % slot.ringDwords);
    slot.pendingDwords = 0;
    slot.deferred.clear();
    slot.watchedFence = 0;
    slot.fenceIndex = 0;
    return 0;
}

int APS5_VABI sceSdmaClose(void* handle) {
    std::lock_guard lock(g_mutex);
    *static_cast<SdmaSlot*>(handle) = SdmaSlot{};
    return 0;
}

int APS5_VABI sceSdmaConstantFill(void* handle, std::uint32_t value, std::uint64_t destination, std::uint32_t bytes, std::uint32_t mode) {
    return ConstantFill(handle, value, destination, bytes, mode, true);
}

int APS5_VABI sceSdmaConstantFillNonBlocking(void* handle, std::uint32_t value, std::uint64_t destination, std::uint32_t bytes, std::uint32_t mode) {
    return ConstantFill(handle, value, destination, bytes, mode, false);
}

int APS5_VABI sceSdmaCopyLinear(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes) {
    return CopyLinear(handle, source, destination, bytes, true);
}

int APS5_VABI sceSdmaCopyLinearNonBlocking(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes) {
    return CopyLinear(handle, source, destination, bytes, false);
}

int APS5_VABI sceSdmaCopyTiled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaCopyTiledNonBlocking(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaCopyWindowL2L(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bitsPerElement, std::uint32_t width,
                                   std::uint32_t height, std::uint32_t sourcePitch, std::uint32_t sourceHeight, std::uint32_t sourceX, std::uint32_t sourceY,
                                   std::uint32_t destinationPitch, std::uint32_t destinationHeight, std::uint32_t destinationX, std::uint32_t destinationY) {
    (void)sourceHeight;
    (void)destinationHeight;
    return CopyWindow(handle, source, destination, bitsPerElement, width, height, sourcePitch, sourceX, sourceY, destinationPitch, destinationX, destinationY, true);
}

int APS5_VABI sceSdmaCopyWindowL2LNonBlocking(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bitsPerElement, std::uint32_t width,
                                              std::uint32_t height, std::uint32_t sourcePitch, std::uint32_t sourceHeight, std::uint32_t sourceX,
                                              std::uint32_t sourceY, std::uint32_t destinationPitch, std::uint32_t destinationHeight, std::uint32_t destinationX,
                                              std::uint32_t destinationY) {
    (void)sourceHeight;
    (void)destinationHeight;
    return CopyWindow(handle, source, destination, bitsPerElement, width, height, sourcePitch, sourceX, sourceY, destinationPitch, destinationX, destinationY, false);
}

int APS5_VABI sceSdmaCopyWindowT2T(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaCopyWindowT2TNonBlocking(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaCopyWindowTiled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaCopyWindowTiledNonBlocking(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaEndQueueMode(void* handle) {
    std::lock_guard lock(g_mutex);
    auto& slot = *static_cast<SdmaSlot*>(handle);
    FlushAll(slot);
    slot.queueMode = false;
    return 0;
}

int APS5_VABI sceSdmaFenceNonBlocking(void* handle) {
    std::lock_guard lock(g_mutex);
    auto& slot = *static_cast<SdmaSlot*>(handle);
    if (static_cast<std::uint64_t>(RingPending(slot)) + FencePacket + 8 > slot.ringDwords) return SdmaErrorRingFull;
    if (slot.fenceIndex >= FenceSlots - 1) return SdmaErrorRingFull;
    slot.watchedFence = EmitFence(slot);
    ++slot.fenceIndex;
    if (!slot.queueMode) Kick(slot);
    return 0;
}

int APS5_VABI sceSdmaFinalize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaFlushAll(void* handle) {
    std::lock_guard lock(g_mutex);
    FlushAll(*static_cast<SdmaSlot*>(handle));
    return 0;
}

int APS5_VABI sceSdmaFlushAllWithEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaKickQueue(void* handle) {
    std::lock_guard lock(g_mutex);
    Kick(*static_cast<SdmaSlot*>(handle));
    return 0;
}

int APS5_VABI sceSdmaOpen(void** handle, void* buffer, std::uint32_t size, const SdmaOpenParam* param) {
    if (!param || size < MinimumWorkingBuffer || !buffer || param->engine - 12 >= 8) return SdmaErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    SdmaSlot& slot = g_slots[param->engine];
    if (slot.open) return SdmaErrorBusy;
    slot = SdmaSlot{};
    slot.open = true;
    slot.ringDwords = size / 4 - FenceSlots;
    if (param->flags & 0x10) slot.writeBack = reinterpret_cast<std::uint64_t*>(param->writeBackAddress);
    *handle = &slot;
    return 0;
}

int APS5_VABI sceSdmaQueryWorkingBufferSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdmaTryFlush(void* handle) {
    std::lock_guard lock(g_mutex);
    auto& slot = *static_cast<SdmaSlot*>(handle);
    if (slot.watchedFence == 0) return 0;
    if (slot.fencesExecuted < slot.watchedFence) return SdmaErrorFencePending;
    slot.watchedFence = 0;
    slot.fenceIndex = 0;
    return 0;
}

int APS5_VABI sceSdmaWaitForEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
