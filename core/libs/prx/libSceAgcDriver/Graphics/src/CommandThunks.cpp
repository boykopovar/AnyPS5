#include "prx/libSceAgcDriver/Graphics/include/CommandList.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t ThunkSlots = 8;

constexpr std::array<std::string_view, CommandCount> Names{
#define APS5_COMMAND_NAME(name) "vk" #name,
    APS5_COMMANDS(APS5_COMMAND_NAME)
#undef APS5_COMMAND_NAME
};

constexpr std::size_t indexOf(CommandId id) {
    return static_cast<std::size_t>(id);
}

std::array<std::array<std::atomic<PFN_vkVoidFunction>, ThunkSlots>, CommandCount> realSlots{};
std::mutex slotMutex;

template<CommandId Id>
struct Command;

#define APS5_COMMAND_TRAIT(name) \
    template<> \
    struct Command<CommandId::name> { \
        using Pfn = PFN_vk##name; \
        static constexpr auto member = &CommandFunctions::name; \
        static constexpr const char* text = "vk" #name; \
    };
APS5_COMMANDS(APS5_COMMAND_TRAIT)
#undef APS5_COMMAND_TRAIT

template<typename T>
const T& unpack(const std::byte* payload) {
    return *std::launder(reinterpret_cast<const T*>(payload));
}

template<typename T>
const T* arrayAt(const std::byte*& at, std::uint32_t count) {
    const auto* values = count != 0 ? reinterpret_cast<const T*>(at) : nullptr;
    at += CommandStream::Aligned(sizeof(T) * count);
    return values;
}

template<typename Header>
const std::byte* afterHeader(const std::byte* payload) {
    return payload + CommandStream::Aligned(sizeof(Header));
}

constexpr VkStructureType Unchained = VK_STRUCTURE_TYPE_MAX_ENUM;

VkStructureType chainOf(const void* next) {
    return next != nullptr ? static_cast<const VkBaseInStructure*>(next)->sType : Unchained;
}

template<typename T>
VkStructureType chained(const T* values, std::uint32_t count) {
    for (std::uint32_t index = 0; index < count; ++index) {
        if (values[index].pNext != nullptr) return chainOf(values[index].pNext);
    }
    return Unchained;
}

template<typename... A>
inline constexpr bool PlainArguments = ((!std::is_pointer_v<A> || !std::is_const_v<std::remove_pointer_t<A>>) && ...);

template<CommandId Id, typename... A>
void runPlain(const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
    std::apply([&](const A&... arguments) { (functions.*Command<Id>::member)(target, arguments...); }, unpack<std::tuple<A...>>(payload));
}

template<CommandId Id, typename... A>
void direct(CommandStream& stream, VkStructureType chain, A... arguments) {
    const auto target = stream.Demote(Command<Id>::text, chain);
    (stream.Functions().*Command<Id>::member)(target, arguments...);
}

template<CommandId Id>
struct Streamer {
    template<typename... A>
    static void Append(CommandStream& stream, A... arguments) {
        if constexpr (PlainArguments<A...>) stream.Emplace(&runPlain<Id, A...>, std::tuple<A...>(arguments...));
        else direct<Id>(stream, Unchained, arguments...);
    }
};

struct RangeHeader {
    std::uint32_t first;
    std::uint32_t count;
};

template<CommandId Id, typename T>
struct RangeStreamer {
    static void Append(CommandStream& stream, std::uint32_t first, std::uint32_t count, const T* values) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = afterHeader<RangeHeader>(payload);
            (functions.*Command<Id>::member)(target, header.first, header.count, arrayAt<T>(at, header.count));
        }, RangeHeader{first, count}, std::span(values, count));
    }
};

template<>
struct Streamer<CommandId::CmdSetViewport> : RangeStreamer<CommandId::CmdSetViewport, VkViewport> {};
template<>
struct Streamer<CommandId::CmdSetScissor> : RangeStreamer<CommandId::CmdSetScissor, VkRect2D> {};
template<>
struct Streamer<CommandId::CmdSetColorWriteMaskEXT> : RangeStreamer<CommandId::CmdSetColorWriteMaskEXT, VkColorComponentFlags> {};

template<CommandId Id, typename T>
struct CountStreamer {
    static void Append(CommandStream& stream, std::uint32_t count, const T* values) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = afterHeader<RangeHeader>(payload);
            (functions.*Command<Id>::member)(target, header.count, arrayAt<T>(at, header.count));
        }, RangeHeader{0, count}, std::span(values, count));
    }
};

template<>
struct Streamer<CommandId::CmdSetViewportWithCount> : CountStreamer<CommandId::CmdSetViewportWithCount, VkViewport> {};
template<>
struct Streamer<CommandId::CmdSetViewportWithCountEXT> : CountStreamer<CommandId::CmdSetViewportWithCountEXT, VkViewport> {};
template<>
struct Streamer<CommandId::CmdSetScissorWithCount> : CountStreamer<CommandId::CmdSetScissorWithCount, VkRect2D> {};
template<>
struct Streamer<CommandId::CmdSetScissorWithCountEXT> : CountStreamer<CommandId::CmdSetScissorWithCountEXT, VkRect2D> {};

struct BindSetsHeader {
    VkPipelineBindPoint bindPoint;
    VkPipelineLayout layout;
    std::uint32_t firstSet;
    std::uint32_t sets;
    std::uint32_t offsets;
};

template<>
struct Streamer<CommandId::CmdBindDescriptorSets> {
    static void Append(CommandStream& stream, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t firstSet, std::uint32_t sets, const VkDescriptorSet* descriptorSets, std::uint32_t offsets, const std::uint32_t* dynamicOffsets) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<BindSetsHeader>(payload);
            const auto* at = afterHeader<BindSetsHeader>(payload);
            const auto* descriptorSets = arrayAt<VkDescriptorSet>(at, header.sets);
            const auto* dynamicOffsets = arrayAt<std::uint32_t>(at, header.offsets);
            functions.CmdBindDescriptorSets(target, header.bindPoint, header.layout, header.firstSet, header.sets, descriptorSets, header.offsets, dynamicOffsets);
        }, BindSetsHeader{bindPoint, layout, firstSet, sets, offsets}, std::span(descriptorSets, sets), std::span(dynamicOffsets, offsets));
    }
};

struct PushHeader {
    VkPipelineLayout layout;
    VkShaderStageFlags stages;
    std::uint32_t offset;
    std::uint32_t size;
};

template<>
struct Streamer<CommandId::CmdPushConstants> {
    static void Append(CommandStream& stream, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<PushHeader>(payload);
            functions.CmdPushConstants(target, header.layout, header.stages, header.offset, header.size, afterHeader<PushHeader>(payload));
        }, PushHeader{layout, stages, offset, size}, std::span(static_cast<const std::byte*>(values), size));
    }
};

template<>
struct Streamer<CommandId::CmdBindVertexBuffers> {
    static void Append(CommandStream& stream, std::uint32_t first, std::uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = afterHeader<RangeHeader>(payload);
            const auto* buffers = arrayAt<VkBuffer>(at, header.count);
            const auto* offsets = arrayAt<VkDeviceSize>(at, header.count);
            functions.CmdBindVertexBuffers(target, header.first, header.count, buffers, offsets);
        }, RangeHeader{first, count}, std::span(buffers, count), std::span(offsets, count));
    }
};

struct BarrierHeader {
    VkPipelineStageFlags source;
    VkPipelineStageFlags destination;
    VkDependencyFlags dependencies;
    std::uint32_t memory;
    std::uint32_t buffers;
    std::uint32_t images;
};

template<>
struct Streamer<CommandId::CmdPipelineBarrier> {
    static void Append(CommandStream& stream, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependencies, std::uint32_t memory, const VkMemoryBarrier* memoryBarriers, std::uint32_t buffers, const VkBufferMemoryBarrier* bufferBarriers, std::uint32_t images, const VkImageMemoryBarrier* imageBarriers) {
        auto chain = chained(memoryBarriers, memory);
        if (chain == Unchained) chain = chained(bufferBarriers, buffers);
        if (chain == Unchained) chain = chained(imageBarriers, images);
        if (chain != Unchained) {
            direct<CommandId::CmdPipelineBarrier>(stream, chain, source, destination, dependencies, memory, memoryBarriers, buffers, bufferBarriers, images, imageBarriers);
            return;
        }
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<BarrierHeader>(payload);
            const auto* at = afterHeader<BarrierHeader>(payload);
            const auto* memoryBarriers = arrayAt<VkMemoryBarrier>(at, header.memory);
            const auto* bufferBarriers = arrayAt<VkBufferMemoryBarrier>(at, header.buffers);
            const auto* imageBarriers = arrayAt<VkImageMemoryBarrier>(at, header.images);
            functions.CmdPipelineBarrier(target, header.source, header.destination, header.dependencies, header.memory, memoryBarriers, header.buffers, bufferBarriers, header.images, imageBarriers);
        }, BarrierHeader{source, destination, dependencies, memory, buffers, images}, std::span(memoryBarriers, memory), std::span(bufferBarriers, buffers), std::span(imageBarriers, images));
    }
};

struct CopyBufferHeader {
    VkBuffer source;
    VkBuffer destination;
    std::uint32_t count;
};

template<>
struct Streamer<CommandId::CmdCopyBuffer> {
    static void Append(CommandStream& stream, VkBuffer source, VkBuffer destination, std::uint32_t count, const VkBufferCopy* regions) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<CopyBufferHeader>(payload);
            const auto* at = afterHeader<CopyBufferHeader>(payload);
            functions.CmdCopyBuffer(target, header.source, header.destination, header.count, arrayAt<VkBufferCopy>(at, header.count));
        }, CopyBufferHeader{source, destination, count}, std::span(regions, count));
    }
};

struct UpdateBufferHeader {
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
};

template<>
struct Streamer<CommandId::CmdUpdateBuffer> {
    static void Append(CommandStream& stream, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<UpdateBufferHeader>(payload);
            functions.CmdUpdateBuffer(target, header.buffer, header.offset, header.size, afterHeader<UpdateBufferHeader>(payload));
        }, UpdateBufferHeader{buffer, offset, size}, std::span(static_cast<const std::byte*>(data), static_cast<std::size_t>(size)));
    }
};

struct BufferImageHeader {
    VkBuffer buffer;
    VkImage image;
    VkImageLayout layout;
    std::uint32_t count;
};

template<>
struct Streamer<CommandId::CmdCopyBufferToImage> {
    static void Append(CommandStream& stream, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<BufferImageHeader>(payload);
            const auto* at = afterHeader<BufferImageHeader>(payload);
            functions.CmdCopyBufferToImage(target, header.buffer, header.image, header.layout, header.count, arrayAt<VkBufferImageCopy>(at, header.count));
        }, BufferImageHeader{source, destination, layout, count}, std::span(regions, count));
    }
};

template<>
struct Streamer<CommandId::CmdCopyImageToBuffer> {
    static void Append(CommandStream& stream, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t count, const VkBufferImageCopy* regions) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<BufferImageHeader>(payload);
            const auto* at = afterHeader<BufferImageHeader>(payload);
            functions.CmdCopyImageToBuffer(target, header.image, header.layout, header.buffer, header.count, arrayAt<VkBufferImageCopy>(at, header.count));
        }, BufferImageHeader{destination, source, layout, count}, std::span(regions, count));
    }
};

struct ImageToImageHeader {
    VkImage source;
    VkImageLayout sourceLayout;
    VkImage destination;
    VkImageLayout destinationLayout;
    std::uint32_t count;
    VkFilter filter;
};

template<>
struct Streamer<CommandId::CmdCopyImage> {
    static void Append(CommandStream& stream, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageCopy* regions) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<ImageToImageHeader>(payload);
            const auto* at = afterHeader<ImageToImageHeader>(payload);
            functions.CmdCopyImage(target, header.source, header.sourceLayout, header.destination, header.destinationLayout, header.count, arrayAt<VkImageCopy>(at, header.count));
        }, ImageToImageHeader{source, sourceLayout, destination, destinationLayout, count, VK_FILTER_NEAREST}, std::span(regions, count));
    }
};

template<>
struct Streamer<CommandId::CmdBlitImage> {
    static void Append(CommandStream& stream, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageBlit* regions, VkFilter filter) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<ImageToImageHeader>(payload);
            const auto* at = afterHeader<ImageToImageHeader>(payload);
            functions.CmdBlitImage(target, header.source, header.sourceLayout, header.destination, header.destinationLayout, header.count, arrayAt<VkImageBlit>(at, header.count), header.filter);
        }, ImageToImageHeader{source, sourceLayout, destination, destinationLayout, count, filter}, std::span(regions, count));
    }
};

struct ClearColorHeader {
    VkImage image;
    VkImageLayout layout;
    VkClearColorValue color;
    std::uint32_t count;
};

template<>
struct Streamer<CommandId::CmdClearColorImage> {
    static void Append(CommandStream& stream, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t count, const VkImageSubresourceRange* ranges) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto& header = unpack<ClearColorHeader>(payload);
            const auto* at = afterHeader<ClearColorHeader>(payload);
            functions.CmdClearColorImage(target, header.image, header.layout, &header.color, header.count, arrayAt<VkImageSubresourceRange>(at, header.count));
        }, ClearColorHeader{image, layout, *color, count}, std::span(ranges, count));
    }
};

struct ClearDepthHeader {
    VkImage image;
    VkImageLayout layout;
    VkClearDepthStencilValue value;
    std::uint32_t count;
};

template<>
struct Streamer<CommandId::CmdClearDepthStencilImage> {
    static void Append(CommandStream& stream, VkImage image, VkImageLayout layout, const VkClearDepthStencilValue* value, std::uint32_t count, const VkImageSubresourceRange* ranges) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto& header = unpack<ClearDepthHeader>(payload);
            const auto* at = afterHeader<ClearDepthHeader>(payload);
            functions.CmdClearDepthStencilImage(target, header.image, header.layout, &header.value, header.count, arrayAt<VkImageSubresourceRange>(at, header.count));
        }, ClearDepthHeader{image, layout, *value, count}, std::span(ranges, count));
    }
};

struct ClearAttachmentsHeader {
    std::uint32_t attachments;
    std::uint32_t rects;
};

template<>
struct Streamer<CommandId::CmdClearAttachments> {
    static void Append(CommandStream& stream, std::uint32_t attachments, const VkClearAttachment* clears, std::uint32_t rects, const VkClearRect* areas) {
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            const auto header = unpack<ClearAttachmentsHeader>(payload);
            const auto* at = afterHeader<ClearAttachmentsHeader>(payload);
            const auto* clears = arrayAt<VkClearAttachment>(at, header.attachments);
            const auto* areas = arrayAt<VkClearRect>(at, header.rects);
            functions.CmdClearAttachments(target, header.attachments, clears, header.rects, areas);
        }, ClearAttachmentsHeader{attachments, rects}, std::span(clears, attachments), std::span(areas, rects));
    }
};

struct BeginPassHeader {
    VkRenderPassBeginInfo info;
    VkSubpassContents contents;
};

template<>
struct Streamer<CommandId::CmdBeginRenderPass> {
    static void Append(CommandStream& stream, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
        if (info->pNext != nullptr) {
            direct<CommandId::CmdBeginRenderPass>(stream, chainOf(info->pNext), info, contents);
            return;
        }
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            auto header = unpack<BeginPassHeader>(payload);
            const auto* at = afterHeader<BeginPassHeader>(payload);
            header.info.pClearValues = arrayAt<VkClearValue>(at, header.info.clearValueCount);
            functions.CmdBeginRenderPass(target, &header.info, header.contents);
        }, BeginPassHeader{*info, contents}, std::span(info->pClearValues, info->clearValueCount));
    }
};

struct RenderingHeader {
    VkRenderingInfoKHR info;
    VkRenderingAttachmentInfoKHR depth;
    VkRenderingAttachmentInfoKHR stencil;
};

template<>
struct Streamer<CommandId::CmdBeginRenderingKHR> {
    static void Append(CommandStream& stream, const VkRenderingInfoKHR* info) {
        auto chain = chainOf(info->pNext);
        if (chain == Unchained && info->pDepthAttachment != nullptr) chain = chainOf(info->pDepthAttachment->pNext);
        if (chain == Unchained && info->pStencilAttachment != nullptr) chain = chainOf(info->pStencilAttachment->pNext);
        if (chain == Unchained) chain = chained(info->pColorAttachments, info->colorAttachmentCount);
        if (chain != Unchained) {
            direct<CommandId::CmdBeginRenderingKHR>(stream, chain, info);
            return;
        }
        stream.Emplace([](const std::byte* payload, VkCommandBuffer target, const CommandFunctions& functions) {
            auto header = unpack<RenderingHeader>(payload);
            const auto* at = afterHeader<RenderingHeader>(payload);
            header.info.pColorAttachments = arrayAt<VkRenderingAttachmentInfoKHR>(at, header.info.colorAttachmentCount);
            if (header.info.pDepthAttachment != nullptr) header.info.pDepthAttachment = &header.depth;
            if (header.info.pStencilAttachment != nullptr) header.info.pStencilAttachment = &header.stencil;
            functions.CmdBeginRenderingKHR(target, &header.info);
        }, RenderingHeader{*info, info->pDepthAttachment != nullptr ? *info->pDepthAttachment : VkRenderingAttachmentInfoKHR{}, info->pStencilAttachment != nullptr ? *info->pStencilAttachment : VkRenderingAttachmentInfoKHR{}}, std::span(info->pColorAttachments, info->colorAttachmentCount));
    }
};

template<CommandId Id, std::size_t K, typename... A>
VKAPI_ATTR void VKAPI_CALL thunk(VkCommandBuffer commands, A... arguments) {
    if (!CommandStream::IsHandle(commands)) [[likely]] {
        reinterpret_cast<typename Command<Id>::Pfn>(realSlots[indexOf(Id)][K].load(std::memory_order_relaxed))(commands, arguments...);
        return;
    }
    auto& stream = CommandStream::From(commands);
    if (const auto target = stream.Materialized(); target != VK_NULL_HANDLE) {
        (stream.Functions().*Command<Id>::member)(target, arguments...);
        return;
    }
    Streamer<Id>::Append(stream, arguments...);
}

[[noreturn]] void notAStream(const char* command) {
    throw std::logic_error(std::string(command) + " of a streamed batch was given a command buffer that is not a stream");
}

template<CommandId Id, typename... A>
VKAPI_ATTR void VKAPI_CALL streamed(VkCommandBuffer commands, A... arguments) {
    if (!CommandStream::IsHandle(commands)) [[unlikely]] notAStream(Command<Id>::text);
    auto& stream = CommandStream::From(commands);
    if (const auto target = stream.Materialized(); target != VK_NULL_HANDLE) {
        (stream.Functions().*Command<Id>::member)(target, arguments...);
        return;
    }
    Streamer<Id>::Append(stream, arguments...);
}

template<CommandId Id, typename... A>
void setStreamed(CommandFunctions& functions, void(VKAPI_PTR*)(VkCommandBuffer, A...)) {
    functions.*Command<Id>::member = &streamed<Id, A...>;
}

template<CommandId Id, std::size_t K, typename... A>
PFN_vkVoidFunction thunkOf(void(VKAPI_PTR*)(VkCommandBuffer, A...)) {
    return reinterpret_cast<PFN_vkVoidFunction>(&thunk<Id, K, A...>);
}

template<CommandId Id, std::size_t... K>
std::array<PFN_vkVoidFunction, ThunkSlots> thunksFor(std::index_sequence<K...>) {
    return {thunkOf<Id, K>(typename Command<Id>::Pfn{})...};
}

const std::array<std::array<PFN_vkVoidFunction, ThunkSlots>, CommandCount>& thunks() {
    static const std::array<std::array<PFN_vkVoidFunction, ThunkSlots>, CommandCount> table{
#define APS5_COMMAND_THUNKS(name) thunksFor<CommandId::name>(std::make_index_sequence<ThunkSlots>{}),
        APS5_COMMANDS(APS5_COMMAND_THUNKS)
#undef APS5_COMMAND_THUNKS
    };
    return table;
}

const std::vector<std::pair<std::string_view, CommandId>>& sortedNames() {
    static const auto sorted = [] {
        std::vector<std::pair<std::string_view, CommandId>> names;
        for (std::size_t index = 0; index < CommandCount; ++index) names.emplace_back(Names[index], static_cast<CommandId>(index));
        std::sort(names.begin(), names.end());
        return names;
    }();
    return sorted;
}

const std::pair<std::string_view, CommandId>* find(std::string_view name) noexcept {
    const auto& names = sortedNames();
    const auto found = std::lower_bound(names.begin(), names.end(), name, [](const auto& entry, std::string_view key) { return entry.first < key; });
    return found != names.end() && found->first == name ? &*found : nullptr;
}

}

std::span<const std::string_view> CommandNames() {
    return Names;
}

bool IsCommandName(std::string_view name) noexcept {
    return name.starts_with("vkCmd");
}

CommandId CommandFor(std::string_view name) {
    if (const auto* found = find(name)) return found->second;
    throw std::logic_error("AGC graphics: " + std::string(name) + " is a Vulkan command without a command-stream thunk; add it to APS5_COMMANDS");
}

const CommandFunctions& StreamedCommands() {
    static const CommandFunctions functions = [] {
        CommandFunctions table{};
#define APS5_COMMAND_STREAMED(name) setStreamed<CommandId::name>(table, typename Command<CommandId::name>::Pfn{});
        APS5_COMMANDS(APS5_COMMAND_STREAMED)
#undef APS5_COMMAND_STREAMED
        return table;
    }();
    return functions;
}

void ResolveCommandFunctions(CommandFunctions& functions, VkDevice device, PFN_vkGetDeviceProcAddr resolve) {
#define APS5_COMMAND_RESOLVE(name) functions.name = reinterpret_cast<PFN_vk##name>(resolve(device, "vk" #name));
    APS5_COMMANDS(APS5_COMMAND_RESOLVE)
#undef APS5_COMMAND_RESOLVE
}

PFN_vkVoidFunction CommandThunk(std::string_view name, PFN_vkVoidFunction real) {
    if (real == nullptr) return nullptr;
    const auto id = indexOf(CommandFor(name));
    auto& slots = realSlots[id];
    for (std::size_t slot = 0; slot < ThunkSlots; ++slot) {
        if (slots[slot].load(std::memory_order_acquire) == real) return thunks()[id][slot];
    }
    std::lock_guard lock(slotMutex);
    for (std::size_t slot = 0; slot < ThunkSlots; ++slot) {
        const auto held = slots[slot].load(std::memory_order_acquire);
        if (held == real) return thunks()[id][slot];
        if (held != nullptr) continue;
        slots[slot].store(real, std::memory_order_release);
        return thunks()[id][slot];
    }
    throw std::runtime_error("AGC graphics: " + std::string(name) + " has more than " + std::to_string(ThunkSlots) + " distinct driver entry points; the command-stream thunks have no slot for another");
}

bool IsCommandThunk(std::string_view name, PFN_vkVoidFunction function) noexcept {
    const auto* found = find(name);
    if (found == nullptr || function == nullptr) return false;
    const auto& row = thunks()[indexOf(found->second)];
    return std::find(row.begin(), row.end(), function) != row.end();
}

std::size_t CommandThunkSlots(CommandId id) noexcept {
    std::size_t used = 0;
    for (const auto& slot : realSlots[indexOf(id)]) used += slot.load(std::memory_order_acquire) != nullptr ? 1 : 0;
    return used;
}

}
