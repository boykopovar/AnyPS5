#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t ChunkBytes = 64 * 1024;
constexpr std::size_t RecordAlign = 16;

struct Record {
    CommandEncoder::Run run;
    std::uint32_t bytes;
};

constexpr std::size_t RecordHeader = (sizeof(Record) + RecordAlign - 1) / RecordAlign * RecordAlign;

constexpr std::size_t aligned(std::size_t bytes) {
    return (bytes + RecordAlign - 1) / RecordAlign * RecordAlign;
}

struct SlotState {
    std::atomic<CommandEncoder*> encoder{nullptr};
    DeviceFunctions real;
    VkDevice device = VK_NULL_HANDLE;
    bool claimed = false;
};

std::array<SlotState, CommandEncoder::Slots>& slots() {
    static std::array<SlotState, CommandEncoder::Slots> value;
    return value;
}

std::mutex& slotMutex() {
    static std::mutex value;
    return value;
}

std::function<void()>& testDelay() {
    static std::function<void()> value;
    return value;
}

template<std::size_t S>
CommandEncoder* deferring(VkCommandBuffer commands) {
    auto* encoder = slots()[S].encoder.load(std::memory_order_acquire);
    if (encoder == nullptr) return nullptr;
    if (encoder->Defers(commands)) return encoder;
    if (encoder->Behind(commands)) encoder->Settle();
    return nullptr;
}

template<typename Packed>
const Packed& unpack(const std::byte* payload) {
    return *std::launder(reinterpret_cast<const Packed*>(payload));
}

template<auto Member, typename... A>
void runPlain(const std::byte* payload, VkCommandBuffer commands, const DeviceFunctions& functions) {
    std::apply([&](const A&... arguments) { (functions.*Member)(commands, arguments...); }, unpack<std::tuple<A...>>(payload));
}

template<std::size_t S, auto Member, typename... A>
VKAPI_ATTR void VKAPI_CALL plain(VkCommandBuffer commands, A... arguments) {
    if (auto* encoder = deferring<S>(commands)) {
        new (encoder->Append(&runPlain<Member, A...>, sizeof(std::tuple<A...>))) std::tuple<A...>(arguments...);
        return;
    }
    (slots()[S].real.*Member)(commands, arguments...);
}

template<std::size_t S, auto Member, typename... A>
constexpr auto plainFor(void(VKAPI_PTR*)(VkCommandBuffer, A...)) {
    return &plain<S, Member, A...>;
}

template<typename T>
std::byte* copyArray(std::byte* to, const T* from, std::uint32_t count) {
    if (count != 0) std::memcpy(to, from, sizeof(T) * count);
    return to + aligned(sizeof(T) * count);
}

template<typename T>
const T* arrayAt(const std::byte*& at, std::uint32_t count) {
    const auto* values = count != 0 ? reinterpret_cast<const T*>(at) : nullptr;
    at += aligned(sizeof(T) * count);
    return values;
}

struct RangeHeader {
    std::uint32_t first;
    std::uint32_t count;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL setViewport(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkViewport* viewports) {
    if (auto* encoder = deferring<S>(commands)) {
        auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            functions.cmdSetViewport(target, header.first, header.count, arrayAt<VkViewport>(at, header.count));
        }, aligned(sizeof(RangeHeader)) + aligned(sizeof(VkViewport) * count));
        new (at) RangeHeader{first, count};
        copyArray(at + aligned(sizeof(RangeHeader)), viewports, count);
        return;
    }
    slots()[S].real.cmdSetViewport(commands, first, count, viewports);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL setScissor(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkRect2D* scissors) {
    if (auto* encoder = deferring<S>(commands)) {
        auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            functions.cmdSetScissor(target, header.first, header.count, arrayAt<VkRect2D>(at, header.count));
        }, aligned(sizeof(RangeHeader)) + aligned(sizeof(VkRect2D) * count));
        new (at) RangeHeader{first, count};
        copyArray(at + aligned(sizeof(RangeHeader)), scissors, count);
        return;
    }
    slots()[S].real.cmdSetScissor(commands, first, count, scissors);
}

struct BindSetsHeader {
    VkPipelineBindPoint bindPoint;
    VkPipelineLayout layout;
    std::uint32_t firstSet;
    std::uint32_t sets;
    std::uint32_t offsets;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL bindDescriptorSets(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t firstSet, std::uint32_t sets, const VkDescriptorSet* descriptorSets, std::uint32_t offsets, const std::uint32_t* dynamicOffsets) {
    if (auto* encoder = deferring<S>(commands)) {
        auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<BindSetsHeader>(payload);
            const auto* at = payload + aligned(sizeof(BindSetsHeader));
            const auto* descriptorSets = arrayAt<VkDescriptorSet>(at, header.sets);
            const auto* dynamicOffsets = arrayAt<std::uint32_t>(at, header.offsets);
            functions.cmdBindDescriptorSets(target, header.bindPoint, header.layout, header.firstSet, header.sets, descriptorSets, header.offsets, dynamicOffsets);
        }, aligned(sizeof(BindSetsHeader)) + aligned(sizeof(VkDescriptorSet) * sets) + aligned(sizeof(std::uint32_t) * offsets));
        new (at) BindSetsHeader{bindPoint, layout, firstSet, sets, offsets};
        copyArray(copyArray(at + aligned(sizeof(BindSetsHeader)), descriptorSets, sets), dynamicOffsets, offsets);
        return;
    }
    slots()[S].real.cmdBindDescriptorSets(commands, bindPoint, layout, firstSet, sets, descriptorSets, offsets, dynamicOffsets);
}

struct PushHeader {
    VkPipelineLayout layout;
    VkShaderStageFlags stages;
    std::uint32_t offset;
    std::uint32_t size;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL pushConstants(VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
    if (auto* encoder = deferring<S>(commands)) {
        auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<PushHeader>(payload);
            functions.cmdPushConstants(target, header.layout, header.stages, header.offset, header.size, payload + aligned(sizeof(PushHeader)));
        }, aligned(sizeof(PushHeader)) + aligned(size));
        new (at) PushHeader{layout, stages, offset, size};
        if (size != 0) std::memcpy(at + aligned(sizeof(PushHeader)), values, size);
        return;
    }
    slots()[S].real.cmdPushConstants(commands, layout, stages, offset, size, values);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL bindVertexBuffers(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets) {
    if (auto* encoder = deferring<S>(commands)) {
        auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            const auto* buffers = arrayAt<VkBuffer>(at, header.count);
            const auto* offsets = arrayAt<VkDeviceSize>(at, header.count);
            functions.cmdBindVertexBuffers(target, header.first, header.count, buffers, offsets);
        }, aligned(sizeof(RangeHeader)) + aligned(sizeof(VkBuffer) * count) + aligned(sizeof(VkDeviceSize) * count));
        new (at) RangeHeader{first, count};
        copyArray(copyArray(at + aligned(sizeof(RangeHeader)), buffers, count), offsets, count);
        return;
    }
    slots()[S].real.cmdBindVertexBuffers(commands, first, count, buffers, offsets);
}

struct BarrierHeader {
    VkPipelineStageFlags source;
    VkPipelineStageFlags destination;
    VkDependencyFlags dependencies;
    std::uint32_t memory;
    std::uint32_t buffers;
    std::uint32_t images;
};

template<typename T>
bool chained(const T* values, std::uint32_t count) {
    for (std::uint32_t index = 0; index < count; ++index) {
        if (values[index].pNext != nullptr) return true;
    }
    return false;
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL pipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependencies, std::uint32_t memory, const VkMemoryBarrier* memoryBarriers, std::uint32_t buffers, const VkBufferMemoryBarrier* bufferBarriers, std::uint32_t images, const VkImageMemoryBarrier* imageBarriers) {
    if (auto* encoder = deferring<S>(commands)) {
        if (!chained(memoryBarriers, memory) && !chained(bufferBarriers, buffers) && !chained(imageBarriers, images)) {
            auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                const auto header = unpack<BarrierHeader>(payload);
                const auto* at = payload + aligned(sizeof(BarrierHeader));
                const auto* memoryBarriers = arrayAt<VkMemoryBarrier>(at, header.memory);
                const auto* bufferBarriers = arrayAt<VkBufferMemoryBarrier>(at, header.buffers);
                const auto* imageBarriers = arrayAt<VkImageMemoryBarrier>(at, header.images);
                functions.cmdPipelineBarrier(target, header.source, header.destination, header.dependencies, header.memory, memoryBarriers, header.buffers, bufferBarriers, header.images, imageBarriers);
            }, aligned(sizeof(BarrierHeader)) + aligned(sizeof(VkMemoryBarrier) * memory) + aligned(sizeof(VkBufferMemoryBarrier) * buffers) + aligned(sizeof(VkImageMemoryBarrier) * images));
            new (at) BarrierHeader{source, destination, dependencies, memory, buffers, images};
            copyArray(copyArray(copyArray(at + aligned(sizeof(BarrierHeader)), memoryBarriers, memory), bufferBarriers, buffers), imageBarriers, images);
            return;
        }
        if (!encoder->Direct()) return;
    }
    slots()[S].real.cmdPipelineBarrier(commands, source, destination, dependencies, memory, memoryBarriers, buffers, bufferBarriers, images, imageBarriers);
}

template<typename Header, typename T>
void appendWithArray(CommandEncoder& encoder, CommandEncoder::Run run, const Header& header, const T* values, std::uint32_t count) {
    auto* at = encoder.Append(run, aligned(sizeof(Header)) + aligned(sizeof(T) * count));
    new (at) Header(header);
    copyArray(at + aligned(sizeof(Header)), values, count);
}

template<typename Header, typename T>
const T* arrayAfter(const std::byte* payload, std::uint32_t count) {
    const auto* at = payload + aligned(sizeof(Header));
    return arrayAt<T>(at, count);
}

struct CopyBufferHeader {
    VkBuffer source;
    VkBuffer destination;
    std::uint32_t count;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL copyBuffer(VkCommandBuffer commands, VkBuffer source, VkBuffer destination, std::uint32_t count, const VkBufferCopy* regions) {
    if (auto* encoder = deferring<S>(commands)) {
        appendWithArray(*encoder, [](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<CopyBufferHeader>(payload);
            functions.cmdCopyBuffer(target, header.source, header.destination, header.count, arrayAfter<CopyBufferHeader, VkBufferCopy>(payload, header.count));
        }, CopyBufferHeader{source, destination, count}, regions, count);
        return;
    }
    slots()[S].real.cmdCopyBuffer(commands, source, destination, count, regions);
}

struct UpdateBufferHeader {
    VkBuffer buffer;
    VkDeviceSize offset;
    std::uint32_t size;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL updateBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    if (auto* encoder = deferring<S>(commands)) {
        appendWithArray(*encoder, [](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<UpdateBufferHeader>(payload);
            functions.cmdUpdateBuffer(target, header.buffer, header.offset, header.size, arrayAfter<UpdateBufferHeader, std::byte>(payload, header.size));
        }, UpdateBufferHeader{buffer, offset, static_cast<std::uint32_t>(size)}, static_cast<const std::byte*>(data), static_cast<std::uint32_t>(size));
        return;
    }
    slots()[S].real.cmdUpdateBuffer(commands, buffer, offset, size, data);
}

struct BufferToImageHeader {
    VkBuffer buffer;
    VkImage image;
    VkImageLayout layout;
    std::uint32_t count;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL copyBufferToImage(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
    if (auto* encoder = deferring<S>(commands)) {
        appendWithArray(*encoder, [](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<BufferToImageHeader>(payload);
            functions.cmdCopyBufferToImage(target, header.buffer, header.image, header.layout, header.count, arrayAfter<BufferToImageHeader, VkBufferImageCopy>(payload, header.count));
        }, BufferToImageHeader{source, destination, layout, count}, regions, count);
        return;
    }
    slots()[S].real.cmdCopyBufferToImage(commands, source, destination, layout, count, regions);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL copyImageToBuffer(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t count, const VkBufferImageCopy* regions) {
    if (auto* encoder = deferring<S>(commands)) {
        appendWithArray(*encoder, [](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<BufferToImageHeader>(payload);
            functions.cmdCopyImageToBuffer(target, header.image, header.layout, header.buffer, header.count, arrayAfter<BufferToImageHeader, VkBufferImageCopy>(payload, header.count));
        }, BufferToImageHeader{destination, source, layout, count}, regions, count);
        return;
    }
    slots()[S].real.cmdCopyImageToBuffer(commands, source, layout, destination, count, regions);
}

struct ClearColorHeader {
    VkImage image;
    VkImageLayout layout;
    VkClearColorValue color;
    std::uint32_t count;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL clearColorImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t count, const VkImageSubresourceRange* ranges) {
    if (auto* encoder = deferring<S>(commands)) {
        appendWithArray(*encoder, [](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<ClearColorHeader>(payload);
            functions.cmdClearColorImage(target, header.image, header.layout, &header.color, header.count, arrayAfter<ClearColorHeader, VkImageSubresourceRange>(payload, header.count));
        }, ClearColorHeader{image, layout, *color, count}, ranges, count);
        return;
    }
    slots()[S].real.cmdClearColorImage(commands, image, layout, color, count, ranges);
}

struct BeginPassHeader {
    VkRenderPassBeginInfo info;
    VkSubpassContents contents;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL beginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    if (auto* encoder = deferring<S>(commands)) {
        if (info->pNext == nullptr) {
            auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                auto header = unpack<BeginPassHeader>(payload);
                const auto* at = payload + aligned(sizeof(BeginPassHeader));
                header.info.pClearValues = arrayAt<VkClearValue>(at, header.info.clearValueCount);
                functions.cmdBeginRenderPass(target, &header.info, header.contents);
            }, aligned(sizeof(BeginPassHeader)) + aligned(sizeof(VkClearValue) * info->clearValueCount));
            new (at) BeginPassHeader{*info, contents};
            copyArray(at + aligned(sizeof(BeginPassHeader)), info->pClearValues, info->clearValueCount);
            return;
        }
        if (!encoder->Direct()) return;
    }
    slots()[S].real.cmdBeginRenderPass(commands, info, contents);
}

struct RenderingHeader {
    VkRenderingInfoKHR info;
    VkRenderingAttachmentInfoKHR depth;
    VkRenderingAttachmentInfoKHR stencil;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL beginRendering(VkCommandBuffer commands, const VkRenderingInfoKHR* info) {
    if (auto* encoder = deferring<S>(commands)) {
        const bool plainDepth = info->pDepthAttachment == nullptr || info->pDepthAttachment->pNext == nullptr;
        const bool plainStencil = info->pStencilAttachment == nullptr || info->pStencilAttachment->pNext == nullptr;
        if (info->pNext == nullptr && plainDepth && plainStencil && !chained(info->pColorAttachments, info->colorAttachmentCount)) {
            auto* at = encoder->Append([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                auto header = unpack<RenderingHeader>(payload);
                const auto* at = payload + aligned(sizeof(RenderingHeader));
                header.info.pColorAttachments = arrayAt<VkRenderingAttachmentInfoKHR>(at, header.info.colorAttachmentCount);
                if (header.info.pDepthAttachment != nullptr) header.info.pDepthAttachment = &header.depth;
                if (header.info.pStencilAttachment != nullptr) header.info.pStencilAttachment = &header.stencil;
                functions.cmdBeginRendering(target, &header.info);
            }, aligned(sizeof(RenderingHeader)) + aligned(sizeof(VkRenderingAttachmentInfoKHR) * info->colorAttachmentCount));
            new (at) RenderingHeader{*info, info->pDepthAttachment != nullptr ? *info->pDepthAttachment : VkRenderingAttachmentInfoKHR{}, info->pStencilAttachment != nullptr ? *info->pStencilAttachment : VkRenderingAttachmentInfoKHR{}};
            copyArray(at + aligned(sizeof(RenderingHeader)), info->pColorAttachments, info->colorAttachmentCount);
            return;
        }
        if (!encoder->Direct()) return;
    }
    slots()[S].real.cmdBeginRendering(commands, info);
}

template<std::size_t S>
void install(DeviceFunctions& table) {
#define APS5_PLAIN(member) \
    if (table.member != nullptr) table.member = plainFor<S, &DeviceFunctions::member>(table.member)
#define APS5_CUSTOM(member, function) \
    if (table.member != nullptr) table.member = &function<S>
    APS5_CUSTOM(cmdPipelineBarrier, pipelineBarrier);
    APS5_CUSTOM(cmdCopyBuffer, copyBuffer);
    APS5_CUSTOM(cmdUpdateBuffer, updateBuffer);
    APS5_PLAIN(cmdFillBuffer);
    APS5_PLAIN(cmdBindPipeline);
    APS5_CUSTOM(cmdBindDescriptorSets, bindDescriptorSets);
    APS5_CUSTOM(cmdPushConstants, pushConstants);
    APS5_PLAIN(cmdDispatch);
    APS5_PLAIN(cmdDispatchIndirect);
    APS5_CUSTOM(cmdBeginRenderPass, beginRenderPass);
    APS5_PLAIN(cmdEndRenderPass);
    APS5_CUSTOM(cmdBeginRendering, beginRendering);
    APS5_PLAIN(cmdEndRendering);
    APS5_CUSTOM(cmdSetViewport, setViewport);
    APS5_CUSTOM(cmdSetScissor, setScissor);
    APS5_PLAIN(cmdSetDepthBounds);
    APS5_PLAIN(cmdSetDepthBias);
    APS5_PLAIN(cmdSetCullMode);
    APS5_PLAIN(cmdSetFrontFace);
    APS5_PLAIN(cmdSetDepthTestEnable);
    APS5_PLAIN(cmdSetDepthWriteEnable);
    APS5_PLAIN(cmdSetDepthCompareOp);
    APS5_PLAIN(cmdSetDepthBoundsTestEnable);
    APS5_PLAIN(cmdSetStencilTestEnable);
    APS5_PLAIN(cmdSetStencilOp);
    APS5_PLAIN(cmdSetStencilCompareMask);
    APS5_PLAIN(cmdSetStencilWriteMask);
    APS5_PLAIN(cmdSetStencilReference);
    APS5_CUSTOM(cmdBindVertexBuffers, bindVertexBuffers);
    APS5_PLAIN(cmdBindIndexBuffer);
    APS5_PLAIN(cmdDraw);
    APS5_PLAIN(cmdDrawIndexed);
    APS5_PLAIN(cmdDrawIndirect);
    APS5_PLAIN(cmdDrawIndexedIndirect);
    APS5_CUSTOM(cmdCopyBufferToImage, copyBufferToImage);
    APS5_CUSTOM(cmdCopyImageToBuffer, copyImageToBuffer);
    APS5_CUSTOM(cmdClearColorImage, clearColorImage);
    APS5_PLAIN(cmdBeginQuery);
    APS5_PLAIN(cmdEndQuery);
#undef APS5_PLAIN
#undef APS5_CUSTOM
}

template<std::size_t... S>
void installAt(std::size_t slot, DeviceFunctions& table, std::index_sequence<S...>) {
    static_cast<void>(((slot == S ? (install<S>(table), true) : false) || ...));
}

}

bool CommandEncoder::Enabled() {
    static const bool enabled = [] {
        const char* inline_ = std::getenv("APS5_RECORD_INLINE");
        return inline_ == nullptr || std::strcmp(inline_, "0") == 0;
    }();
    return enabled;
}

std::size_t CommandEncoder::claim() {
    std::lock_guard lock(slotMutex());
    for (std::size_t index = 0; index < Slots; ++index) {
        if (slots()[index].claimed) continue;
        slots()[index].claimed = true;
        return index;
    }
    return Slots;
}

std::unique_ptr<CommandEncoder> CommandEncoder::Attach(VkDevice device, DeviceFunctions& table) {
    const auto slot = claim();
    if (slot == Slots) return nullptr;
    try {
        return std::unique_ptr<CommandEncoder>(new CommandEncoder(device, table, slot));
    } catch (...) {
        std::lock_guard lock(slotMutex());
        slots()[slot].claimed = false;
        throw;
    }
}

CommandEncoder::CommandEncoder(VkDevice device, DeviceFunctions& table, std::size_t slot) : table(table), real(table), device(device), slot(slot) {
    static_cast<void>(HandoffSpinWindow());
    worker = std::thread([this] { run(); });
    std::lock_guard lock(slotMutex());
    slots()[slot].real = real;
    slots()[slot].device = device;
    installAt(slot, table, std::make_index_sequence<Slots>{});
    slots()[slot].encoder.store(this, std::memory_order_release);
}

CommandEncoder::~CommandEncoder() {
    Quiesce();
    stopping.store(true, std::memory_order_seq_cst);
    posted.fetch_add(1, std::memory_order_seq_cst);
    posted.notify_all();
    worker.join();
    delete filling;
    for (auto* chunk = spare; chunk != nullptr;) delete std::exchange(chunk, chunk->next);
    std::lock_guard lock(slotMutex());
    table = real;
    slots()[slot].encoder.store(nullptr, std::memory_order_release);
    slots()[slot].claimed = false;
}

void CommandEncoder::Recording::deliver() {
    reported = true;
    std::rethrow_exception(error);
}

void CommandEncoder::Recording::refuse() {
    if (!reported) deliver();
    throw std::runtime_error("AGC graphics: the encoder failed to record this command buffer, so it is not submitted");
}

void CommandEncoder::Open(VkCommandBuffer commands, Recording& recording) {
    if (recent.load(std::memory_order_relaxed) != commands) Quiesce();
    windows.push_back({commands, &recording});
    owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
    this->recording = &recording;
    recent.store(commands, std::memory_order_relaxed);
    target.store(commands, std::memory_order_release);
}

void CommandEncoder::RefuseForeignProducer() {
    throw std::logic_error("AGC graphics: another thread used the encoder while an encoding window was open");
}

void CommandEncoder::Close() {
    post();
    const auto closed = windows.back().commands;
    windows.pop_back();
    if (windows.empty()) {
        target.store(VK_NULL_HANDLE, std::memory_order_relaxed);
        recording = nullptr;
        return;
    }
    if (windows.back().commands != closed) await();
    recording = windows.back().recording;
    recent.store(windows.back().commands, std::memory_order_relaxed);
    target.store(windows.back().commands, std::memory_order_relaxed);
}

std::byte* CommandEncoder::Append(Run run, std::size_t bytes) {
    const auto need = RecordHeader + aligned(bytes);
    if (filling != nullptr && (filling->recording != recording || filling->used + need > filling->bytes.size())) post();
    if (filling == nullptr) {
        {
            std::lock_guard lock(queueMutex);
            if (spare != nullptr) filling = std::exchange(spare, spare->next);
        }
        if (filling == nullptr) filling = new Chunk;
        try {
            if (filling->bytes.size() < std::max(ChunkBytes, need)) filling->bytes.resize(std::max(ChunkBytes, need));
        } catch (...) {
            auto* chunk = std::exchange(filling, nullptr);
            std::lock_guard lock(queueMutex);
            chunk->next = std::exchange(spare, chunk);
            throw;
        }
        filling->used = 0;
        filling->records = 0;
        filling->next = nullptr;
        filling->target = target.load(std::memory_order_relaxed);
        filling->recording = recording;
    }
    auto* at = filling->bytes.data() + filling->used;
    new (at) Record{run, static_cast<std::uint32_t>(need)};
    filling->used += need;
    ++filling->records;
    return at + RecordHeader;
}

void CommandEncoder::post() noexcept {
    if (filling == nullptr || filling->records == 0) return;
    const auto records = filling->records;
    commandCount.fetch_add(records, std::memory_order_relaxed);
    postCount.fetch_add(1, std::memory_order_relaxed);
    if (PerformanceTrace::Recording()) {
        if (auto* thread = PerformanceTrace::Thread()) {
            thread->Count(TraceFormat::EncoderPostScope);
            thread->Count(TraceFormat::EncoderCommandScope, records);
        }
    }
    {
        std::lock_guard lock(queueMutex);
        auto* chunk = std::exchange(filling, nullptr);
        (tail != nullptr ? tail->next : head) = chunk;
        tail = chunk;
    }
    posted.fetch_add(1, std::memory_order_seq_cst);
    if (sleeping.load(std::memory_order_seq_cst)) posted.notify_one();
}

void CommandEncoder::Quiesce() noexcept {
    post();
    await();
}

bool CommandEncoder::Direct() noexcept {
    Quiesce();
    return recording == nullptr || !recording->Failed();
}

void CommandEncoder::Settle() noexcept {
    await();
}

void CommandEncoder::await() noexcept {
    const auto through = posted.load(std::memory_order_acquire);
    if (executed.load(std::memory_order_acquire) >= through) return;
    const auto start = std::chrono::steady_clock::now();
    if (!SpinUntil(HandoffSpinWindow(), [&] { return executed.load(std::memory_order_acquire) >= through; })) {
        for (auto seen = executed.load(std::memory_order_acquire); seen < through; seen = executed.load(std::memory_order_acquire)) executed.wait(seen, std::memory_order_acquire);
    }
    const auto end = std::chrono::steady_clock::now();
    waitCount.fetch_add(1, std::memory_order_relaxed);
    waitNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()), std::memory_order_relaxed);
    if (PerformanceTrace::Recording()) {
        if (auto* thread = PerformanceTrace::Thread()) thread->Add(TraceFormat::EncoderWaitScope, nullptr, start, end);
    }
}

void CommandEncoder::execute(Chunk& chunk) {
    const auto start = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(poolMutex);
        if (testDelay()) testDelay()();
        for (std::size_t at = 0; at < chunk.used;) {
            const auto record = *std::launder(reinterpret_cast<const Record*>(chunk.bytes.data() + at));
            record.run(chunk.bytes.data() + at + RecordHeader, chunk.target, real);
            at += record.bytes;
        }
    }
    encodeNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
}

void CommandEncoder::run() {
    std::uint64_t taken = 0;
    for (;;) {
        if (posted.load(std::memory_order_acquire) == taken) {
            if (!SpinUntil(HandoffSpinWindow(), [&] { return posted.load(std::memory_order_acquire) != taken; })) {
                sleeping.store(true, std::memory_order_seq_cst);
                if (posted.load(std::memory_order_seq_cst) == taken) posted.wait(taken, std::memory_order_acquire);
                sleeping.store(false, std::memory_order_relaxed);
            }
            continue;
        }
        Chunk* chunk = nullptr;
        {
            std::lock_guard lock(queueMutex);
            if (head != nullptr) {
                chunk = std::exchange(head, head->next);
                if (head == nullptr) tail = nullptr;
            }
        }
        ++taken;
        if (chunk == nullptr) {
            if (stopping.load(std::memory_order_acquire)) return;
            continue;
        }
        auto& recording = *chunk->recording;
        if (!recording.failed.load(std::memory_order_relaxed)) {
            try {
                execute(*chunk);
            } catch (...) {
                recording.error = std::current_exception();
                recording.failed.store(true, std::memory_order_release);
            }
        }
        {
            std::lock_guard lock(queueMutex);
            chunk->next = std::exchange(spare, chunk);
        }
        executed.fetch_add(1, std::memory_order_release);
        executed.notify_all();
    }
}

CommandEncoder::Statistics CommandEncoder::Counts() const {
    return {postCount.load(std::memory_order_relaxed), commandCount.load(std::memory_order_relaxed), waitCount.load(std::memory_order_relaxed), waitNs.load(std::memory_order_relaxed), encodeNs.load(std::memory_order_relaxed)};
}

void CommandEncoder::SetDelayForTesting(std::function<void()> delay) {
    testDelay() = std::move(delay);
}

void BeforeCommandLookup(VkDevice device) {
    for (auto& state : slots()) {
        auto* encoder = state.encoder.load(std::memory_order_acquire);
        if (encoder == nullptr || state.device != device) continue;
        if (encoder->OpenOnThisThread()) encoder->Quiesce();
        else encoder->Settle();
    }
}

}
