#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#define AGC_COMMAND_STREAM_OPS(X) \
    X(BeginQuery) \
    X(BeginRenderPass) \
    X(BeginRenderingKHR) \
    X(BindDescriptorSets) \
    X(BindIndexBuffer) \
    X(BindPipeline) \
    X(BindVertexBuffers) \
    X(BlitImage) \
    X(ClearColorImage) \
    X(ClearDepthStencilImage) \
    X(CopyBuffer) \
    X(CopyBufferToImage) \
    X(CopyImage) \
    X(CopyImageToBuffer) \
    X(CopyQueryPoolResults) \
    X(Dispatch) \
    X(DispatchIndirect) \
    X(Draw) \
    X(DrawIndexed) \
    X(DrawIndexedIndirect) \
    X(DrawIndexedIndirectCountKHR) \
    X(DrawIndirect) \
    X(DrawIndirectCountKHR) \
    X(DrawMeshTasksEXT) \
    X(DrawMeshTasksIndirectEXT) \
    X(EndQuery) \
    X(EndRenderPass) \
    X(EndRenderingKHR) \
    X(FillBuffer) \
    X(PipelineBarrier) \
    X(PushConstants) \
    X(PushDescriptorSetKHR) \
    X(ResetQueryPool) \
    X(ResolveImage) \
    X(SetCheckpointNV) \
    X(SetCullModeEXT) \
    X(SetDepthBias) \
    X(SetDepthBounds) \
    X(SetDepthBoundsTestEnableEXT) \
    X(SetDepthCompareOpEXT) \
    X(SetDepthTestEnableEXT) \
    X(SetDepthWriteEnableEXT) \
    X(SetFrontFaceEXT) \
    X(SetScissor) \
    X(SetStencilCompareMask) \
    X(SetStencilOpEXT) \
    X(SetStencilReference) \
    X(SetStencilTestEnableEXT) \
    X(SetStencilWriteMask) \
    X(SetViewport) \
    X(UpdateBuffer) \
    X(WriteBufferMarkerAMD) \
    X(WriteTimestamp)

namespace AgcDriver::Graphics {

struct CommandStream::Chunk {
    Chunk* next;
    std::size_t capacity;

    std::byte* Data() { return reinterpret_cast<std::byte*>(this + 1); }
};

struct CommandStreamWriter {
    using Record = CommandStream::Record;

    static CommandStream& StreamOf(VkCommandBuffer commands) {
        return *reinterpret_cast<CommandStream*>(reinterpret_cast<std::uintptr_t>(commands) & CommandStream::HandleAddressMask);
    }

    static void* Allocate(CommandStream& stream, std::size_t bytes) { return stream.allocate(bytes); }
    static void Link(CommandStream& stream, Record* record) { stream.link(record); }
    static bool Closed(const CommandStream& stream) { return stream.closed; }
};

namespace {

using Record = CommandStreamWriter::Record;

enum class CommandOp : std::uint8_t {
#define AGC_COMMAND_STREAM_OP(name) name,
    AGC_COMMAND_STREAM_OPS(AGC_COMMAND_STREAM_OP)
#undef AGC_COMMAND_STREAM_OP
};

constexpr std::array OpNames{
#define AGC_COMMAND_STREAM_OP(name) std::string_view("vkCmd" #name),
    AGC_COMMAND_STREAM_OPS(AGC_COMMAND_STREAM_OP)
#undef AGC_COMMAND_STREAM_OP
};

constexpr std::size_t OpCount = OpNames.size();
constexpr std::size_t SlotCount = 8;

static_assert(std::is_sorted(OpNames.begin(), OpNames.end()), "the command stream ops are looked up by binary search");
static_assert(OpCount <= 255u);
static_assert(sizeof(std::uintptr_t) == 8u, "stream handles carry the stream's address");

template<CommandOp Op>
struct OpFunction;

#define AGC_COMMAND_STREAM_OP(name) \
    template<> \
    struct OpFunction<CommandOp::name> { \
        using Type = PFN_vkCmd##name; \
    };
AGC_COMMAND_STREAM_OPS(AGC_COMMAND_STREAM_OP)
#undef AGC_COMMAND_STREAM_OP

template<typename TFunction>
struct Signature;

template<typename... TArguments>
struct Signature<void (VKAPI_PTR*)(VkCommandBuffer, TArguments...)> {
    using Arguments = std::tuple<TArguments...>;
    static constexpr bool DataPointers = ((std::is_pointer_v<TArguments> && std::is_const_v<std::remove_pointer_t<TArguments>>) || ...);
};

template<CommandOp Op>
using FunctionOf = typename OpFunction<Op>::Type;

template<CommandOp Op>
using ArgumentsOf = typename Signature<FunctionOf<Op>>::Arguments;

template<CommandOp Op>
struct OpRecord : Record {
    ArgumentsOf<Op> arguments;
};

std::array<std::array<std::atomic<PFN_vkVoidFunction>, SlotCount>, OpCount> realFunctions{};

constexpr std::size_t indexOf(CommandOp op) {
    return static_cast<std::size_t>(op);
}

PFN_vkVoidFunction realFunction(CommandOp op, std::size_t slot) {
    return realFunctions[indexOf(op)][slot].load(std::memory_order_relaxed);
}

[[noreturn]] void refuse(CommandOp op, const std::string& reason) {
    throw std::runtime_error("AGC graphics: " + std::string(OpNames[indexOf(op)]) + " " + reason);
}

template<typename T>
T* copy(CommandStream& stream, const T* source, std::uint64_t count) {
    if (source == nullptr || count == 0) return nullptr;
    auto* const target = static_cast<T*>(CommandStreamWriter::Allocate(stream, sizeof(T) * count));
    std::memcpy(static_cast<void*>(target), source, sizeof(T) * count);
    return target;
}

const void* copyBytes(CommandStream& stream, const void* source, std::size_t bytes) {
    if (source == nullptr || bytes == 0) return nullptr;
    auto* const target = CommandStreamWriter::Allocate(stream, bytes);
    std::memcpy(target, source, bytes);
    return target;
}

const void* copyChain(CommandStream& stream, const void* next, CommandOp op);

template<typename T>
T* copyStructs(CommandStream& stream, const T* source, std::uint64_t count, CommandOp op) {
    auto* const target = copy(stream, source, count);
    for (std::uint64_t index = 0; target != nullptr && index < count; ++index) target[index].pNext = copyChain(stream, target[index].pNext, op);
    return target;
}

void copySampleLocations(CommandStream& stream, VkSampleLocationsInfoEXT& info, CommandOp op) {
    info.pNext = copyChain(stream, info.pNext, op);
    info.pSampleLocations = copy(stream, info.pSampleLocations, info.sampleLocationsCount);
}

const void* copyChain(CommandStream& stream, const void* next, CommandOp op) {
    if (next == nullptr) return nullptr;
    const auto type = static_cast<const VkBaseInStructure*>(next)->sType;
    if (type == VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT) {
        auto* const locations = copy(stream, static_cast<const VkSampleLocationsInfoEXT*>(next), 1);
        copySampleLocations(stream, *locations, op);
        return locations;
    }
    if (type == VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT) {
        auto* const begin = copy(stream, static_cast<const VkRenderPassSampleLocationsBeginInfoEXT*>(next), 1);
        begin->pNext = copyChain(stream, begin->pNext, op);
        auto* const attachments = copy(stream, begin->pAttachmentInitialSampleLocations, begin->attachmentInitialSampleLocationsCount);
        for (std::uint32_t index = 0; attachments != nullptr && index < begin->attachmentInitialSampleLocationsCount; ++index) copySampleLocations(stream, attachments[index].sampleLocationsInfo, op);
        begin->pAttachmentInitialSampleLocations = attachments;
        auto* const subpasses = copy(stream, begin->pPostSubpassSampleLocations, begin->postSubpassSampleLocationsCount);
        for (std::uint32_t index = 0; subpasses != nullptr && index < begin->postSubpassSampleLocationsCount; ++index) copySampleLocations(stream, subpasses[index].sampleLocationsInfo, op);
        begin->pPostSubpassSampleLocations = subpasses;
        return begin;
    }
    refuse(op, "chains a structure of type " + std::to_string(type) + ", which a command stream cannot copy");
}

void copyDescriptors(CommandStream& stream, VkWriteDescriptorSet& write, CommandOp op) {
    const auto* const images = write.pImageInfo;
    const auto* const buffers = write.pBufferInfo;
    const auto* const views = write.pTexelBufferView;
    write.pImageInfo = nullptr;
    write.pBufferInfo = nullptr;
    write.pTexelBufferView = nullptr;
    switch (write.descriptorType) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        write.pImageInfo = copy(stream, images, write.descriptorCount);
        return;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        write.pTexelBufferView = copy(stream, views, write.descriptorCount);
        return;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        write.pBufferInfo = copy(stream, buffers, write.descriptorCount);
        return;
    default:
        refuse(op, "pushes descriptor type " + std::to_string(write.descriptorType) + ", which a command stream cannot copy");
    }
}

template<CommandOp Op>
void copyArguments(CommandStream& stream, ArgumentsOf<Op>& arguments) {
    if constexpr (Op == CommandOp::BeginRenderPass) {
        auto& [info, contents] = arguments;
        auto* const begin = copyStructs(stream, info, 1, Op);
        if (begin != nullptr) begin->pClearValues = copy(stream, begin->pClearValues, begin->clearValueCount);
        info = begin;
    } else if constexpr (Op == CommandOp::BeginRenderingKHR) {
        auto& [info] = arguments;
        auto* const rendering = copyStructs(stream, info, 1, Op);
        if (rendering != nullptr) {
            const auto* const depth = rendering->pDepthAttachment;
            rendering->pColorAttachments = copyStructs(stream, rendering->pColorAttachments, rendering->colorAttachmentCount, Op);
            rendering->pDepthAttachment = copyStructs(stream, depth, 1, Op);
            rendering->pStencilAttachment = rendering->pStencilAttachment == depth ? rendering->pDepthAttachment : copyStructs(stream, rendering->pStencilAttachment, 1, Op);
        }
        info = rendering;
    } else if constexpr (Op == CommandOp::BindDescriptorSets) {
        auto& [bindPoint, layout, firstSet, setCount, sets, offsetCount, offsets] = arguments;
        sets = copy(stream, sets, setCount);
        offsets = copy(stream, offsets, offsetCount);
    } else if constexpr (Op == CommandOp::BindVertexBuffers) {
        auto& [firstBinding, bindingCount, buffers, offsets] = arguments;
        buffers = copy(stream, buffers, bindingCount);
        offsets = copy(stream, offsets, bindingCount);
    } else if constexpr (Op == CommandOp::BlitImage || Op == CommandOp::CopyImage || Op == CommandOp::ResolveImage) {
        auto& regions = std::get<5>(arguments);
        regions = copy(stream, regions, std::get<4>(arguments));
    } else if constexpr (Op == CommandOp::ClearColorImage || Op == CommandOp::ClearDepthStencilImage) {
        auto& [image, layout, value, rangeCount, ranges] = arguments;
        value = copy(stream, value, 1);
        ranges = copy(stream, ranges, rangeCount);
    } else if constexpr (Op == CommandOp::CopyBuffer) {
        auto& [source, destination, regionCount, regions] = arguments;
        regions = copy(stream, regions, regionCount);
    } else if constexpr (Op == CommandOp::CopyBufferToImage || Op == CommandOp::CopyImageToBuffer) {
        auto& regions = std::get<4>(arguments);
        regions = copy(stream, regions, std::get<3>(arguments));
    } else if constexpr (Op == CommandOp::PipelineBarrier) {
        auto& [sourceStages, destinationStages, dependencies, memoryCount, memory, bufferCount, buffers, imageCount, images] = arguments;
        memory = copyStructs(stream, memory, memoryCount, Op);
        buffers = copyStructs(stream, buffers, bufferCount, Op);
        images = copyStructs(stream, images, imageCount, Op);
    } else if constexpr (Op == CommandOp::PushConstants) {
        auto& [layout, stages, offset, size, values] = arguments;
        values = copyBytes(stream, values, size);
    } else if constexpr (Op == CommandOp::PushDescriptorSetKHR) {
        auto& [bindPoint, layout, set, writeCount, writes] = arguments;
        auto* const pushed = copyStructs(stream, writes, writeCount, Op);
        for (std::uint32_t index = 0; pushed != nullptr && index < writeCount; ++index) copyDescriptors(stream, pushed[index], Op);
        writes = pushed;
    } else if constexpr (Op == CommandOp::SetScissor || Op == CommandOp::SetViewport) {
        auto& [first, count, values] = arguments;
        values = copy(stream, values, count);
    } else if constexpr (Op == CommandOp::UpdateBuffer) {
        auto& [buffer, offset, size, data] = arguments;
        data = copyBytes(stream, data, static_cast<std::size_t>(size));
    } else if constexpr (Op != CommandOp::SetCheckpointNV) {
        static_assert(!Signature<FunctionOf<Op>>::DataPointers, "a command that reads through a pointer needs its deep copy here");
    }
}

template<CommandOp Op, typename... TArguments>
[[gnu::noinline]] void append(CommandStream& stream, std::size_t slot, TArguments... arguments) {
    static_assert(std::is_trivially_destructible_v<OpRecord<Op>>);
    if (CommandStreamWriter::Closed(stream)) refuse(Op, "was recorded into a command stream after its batch was closed");
    auto* const record = new (CommandStreamWriter::Allocate(stream, sizeof(OpRecord<Op>))) OpRecord<Op>{{nullptr, static_cast<std::uint8_t>(Op), static_cast<std::uint8_t>(slot)}, ArgumentsOf<Op>(arguments...)};
    copyArguments<Op>(stream, record->arguments);
    CommandStreamWriter::Link(stream, record);
}

template<CommandOp Op, std::size_t Slot, typename... TArguments>
VKAPI_ATTR void VKAPI_CALL thunk(VkCommandBuffer commands, TArguments... arguments) {
    if (CommandStream::IsHandle(commands)) {
        append<Op>(CommandStreamWriter::StreamOf(commands), Slot, arguments...);
        return;
    }
    reinterpret_cast<FunctionOf<Op>>(realFunction(Op, Slot))(commands, arguments...);
}

template<CommandOp Op, typename... TArguments, std::size_t... Slots>
std::array<PFN_vkVoidFunction, SlotCount> thunksOf(void (VKAPI_PTR*)(VkCommandBuffer, TArguments...), std::index_sequence<Slots...>) {
    return {reinterpret_cast<PFN_vkVoidFunction>(&thunk<Op, Slots, TArguments...>)...};
}

const std::array<std::array<PFN_vkVoidFunction, SlotCount>, OpCount>& thunkTable() {
    static const std::array<std::array<PFN_vkVoidFunction, SlotCount>, OpCount> table{{
#define AGC_COMMAND_STREAM_OP(name) thunksOf<CommandOp::name>(static_cast<PFN_vkCmd##name>(nullptr), std::make_index_sequence<SlotCount>{}),
        AGC_COMMAND_STREAM_OPS(AGC_COMMAND_STREAM_OP)
#undef AGC_COMMAND_STREAM_OP
    }};
    return table;
}

template<CommandOp Op>
void replayOne(VkCommandBuffer commands, const Record& base) {
    const auto& record = static_cast<const OpRecord<Op>&>(base);
    const auto function = reinterpret_cast<FunctionOf<Op>>(realFunction(Op, base.slot));
    std::apply([&](const auto&... arguments) { function(commands, arguments...); }, record.arguments);
}

std::size_t opOf(const char* name) {
    if (std::strncmp(name, "vkCmd", 5) != 0) return OpCount;
    const std::string_view wanted(name);
    const auto found = std::lower_bound(OpNames.begin(), OpNames.end(), wanted);
    if (found == OpNames.end() || *found != wanted) throw std::runtime_error(std::string("AGC graphics: ") + name + " has no command stream thunk, so it would pass a stream handle to the driver");
    return static_cast<std::size_t>(found - OpNames.begin());
}

PFN_vkVoidFunction thunkFor(std::size_t op, PFN_vkVoidFunction real) {
    const auto& thunks = thunkTable()[op];
    if (std::find(thunks.begin(), thunks.end(), real) != thunks.end()) return real;
    auto& slots = realFunctions[op];
    for (std::size_t slot = 0; slot < SlotCount; ++slot) {
        auto stored = slots[slot].load(std::memory_order_acquire);
        if (stored == nullptr && slots[slot].compare_exchange_strong(stored, real, std::memory_order_acq_rel)) return thunks[slot];
        if (stored == real) return thunks[slot];
    }
    throw std::runtime_error("AGC graphics: " + std::string(OpNames[op]) + " resolved to more than " + std::to_string(SlotCount) + " different entry points");
}

}

CommandStream::CommandStream() {
    const auto address = reinterpret_cast<std::uintptr_t>(this);
    Require((address & ~HandleAddressMask) == 0, "a command stream lies at an address its handle cannot carry");
    handle = reinterpret_cast<VkCommandBuffer>(address | HandleTag);
}

CommandStream::~CommandStream() {
    freeChunks(chunks);
}

bool CommandStream::Records(const char* name) {
    return name != nullptr && std::binary_search(OpNames.begin(), OpNames.end(), std::string_view(name));
}

void CommandStream::Replay(VkCommandBuffer commands) const {
    Require(!IsHandle(commands), "a command stream cannot replay into a stream handle");
    for (const auto* record = first; record != nullptr; record = record->next) {
        switch (static_cast<CommandOp>(record->op)) {
#define AGC_COMMAND_STREAM_OP(name) \
        case CommandOp::name: \
            replayOne<CommandOp::name>(commands, *record); \
            break;
            AGC_COMMAND_STREAM_OPS(AGC_COMMAND_STREAM_OP)
#undef AGC_COMMAND_STREAM_OP
        }
    }
}

void CommandStream::Reset() {
    std::size_t kept = 0;
    auto** retained = &chunks;
    while (*retained != nullptr && (kept == 0 || kept + (*retained)->capacity <= RetainedBytes)) {
        kept += (*retained)->capacity;
        retained = &(*retained)->next;
    }
    freeChunks(*retained);
    *retained = nullptr;
    reserved = kept;
    unused = &chunks;
    cursor = nullptr;
    limit = nullptr;
    first = nullptr;
    tail = &first;
    ops = 0;
    used = 0;
    closed = false;
}

void* CommandStream::grow(std::size_t bytes) {
    auto* chunk = *unused;
    if (bytes > LargeBytes) {
        if (chunk != nullptr && chunk->capacity >= bytes) *unused = chunk->next;
        else chunk = makeChunk(bytes, nullptr);
        chunk->next = chunks;
        chunks = chunk;
        if (unused == &chunks) unused = &chunk->next;
        return chunk->Data();
    }
    if (chunk == nullptr || chunk->capacity < bytes) {
        chunk = makeChunk(ChunkBytes - sizeof(Chunk), chunk);
        *unused = chunk;
    }
    unused = &chunk->next;
    cursor = chunk->Data() + bytes;
    limit = chunk->Data() + chunk->capacity;
    return chunk->Data();
}

CommandStream::Chunk* CommandStream::makeChunk(std::size_t capacity, Chunk* next) {
    auto* const chunk = new (::operator new(sizeof(Chunk) + capacity)) Chunk{next, capacity};
    reserved += capacity;
    return chunk;
}

void CommandStream::freeChunks(Chunk* chunk) {
    while (chunk != nullptr) {
        auto* const next = chunk->next;
        chunk->~Chunk();
        ::operator delete(static_cast<void*>(chunk));
        chunk = next;
    }
}

PFN_vkVoidFunction ResolveDeviceFunction(VkDevice device, PFN_vkGetDeviceProcAddr deviceProc, const char* name) {
    const auto op = opOf(name);
    ++DeviceProcLookups();
    const auto function = deviceProc(device, name);
    if (function == nullptr || op == OpCount) return function;
    return thunkFor(op, function);
}

}
