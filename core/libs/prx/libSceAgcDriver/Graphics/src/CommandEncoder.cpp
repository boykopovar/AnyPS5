#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t ChunkBytes = 64 * 1024;
constexpr std::size_t PooledChunks = 16;
constexpr std::uint64_t PostSampleEvery = 16;
constexpr std::size_t PrintedSites = 16;
constexpr std::chrono::seconds SitePrintInterval{10};

bool SiteProfileRequested() {
    const char* configured = std::getenv("APS5_PROFILE_ENCODER");
    return configured != nullptr && std::strcmp(configured, "0") != 0;
}
constexpr long long LongestIdleSpinUs = 10000;

std::chrono::nanoseconds EncoderIdleSpin() {
    static const auto spin = [] {
        const char* configured = std::getenv("APS5_ENCODER_IDLE_SPIN_US");
        if (configured == nullptr) return std::chrono::nanoseconds::zero();
        return std::chrono::nanoseconds(std::chrono::microseconds(std::clamp<long long>(std::strtoll(configured, nullptr, 10), 0, LongestIdleSpinUs)));
    }();
    return spin;
}
constexpr std::size_t RecordAlign = CommandEncoder::RecordAlign;

struct Record {
    CommandEncoder::Run run;
    std::uint32_t bytes;
};

constexpr std::size_t RecordHeader = (sizeof(Record) + RecordAlign - 1) / RecordAlign * RecordAlign;

constexpr std::size_t aligned(std::size_t bytes) {
    return CommandEncoder::Aligned(bytes);
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

bool sameSite(const CommandEncoder::WaitSite& left, const CommandEncoder::WaitSite& right) {
    if (left.kind != right.kind || left.line != right.line) return false;
    if (left.detail == right.detail) return true;
    return left.detail != nullptr && right.detail != nullptr && std::strcmp(left.detail, right.detail) == 0;
}

std::string_view shortFunction(std::string_view name) {
    if (const auto close = name.rfind(')'); close != std::string_view::npos) {
        int depth = 0;
        for (auto at = close + 1; at-- > 0;) {
            if (name[at] == ')') ++depth;
            else if (name[at] == '(' && --depth == 0) {
                name = name.substr(0, at);
                break;
            }
        }
    }
    int depth = 0;
    for (auto at = name.size(); at-- > 0;) {
        if (name[at] == ')' || name[at] == '>') ++depth;
        else if (name[at] == '(' || name[at] == '<') --depth;
        else if (name[at] == ' ' && depth == 0) {
            name = name.substr(at + 1);
            break;
        }
    }
    const auto last = name.rfind("::");
    if (last == std::string_view::npos || last == 0) return name;
    const auto previous = name.rfind("::", last - 1);
    return previous == std::string_view::npos ? name : name.substr(previous + 2);
}

template<std::size_t S>
CommandEncoder* deferring(VkCommandBuffer commands, const char* command) {
    auto* encoder = slots()[S].encoder.load(std::memory_order_acquire);
    if (encoder == nullptr) return nullptr;
    if (encoder->Defers(commands)) return encoder;
    if (encoder->Behind(commands)) encoder->Settle(CommandEncoder::WaitSite{CommandEncoder::Wait::Behind, command, 0});
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

template<std::size_t S, auto Member, typename Name, typename... A>
VKAPI_ATTR void VKAPI_CALL plain(VkCommandBuffer commands, A... arguments) {
    CommandEncoder::CheckPlain<A...>();
    if (auto* encoder = deferring<S>(commands, Name::Text())) {
        encoder->Emplace(&runPlain<Member, A...>, std::tuple<A...>(arguments...));
        return;
    }
    (slots()[S].real.*Member)(commands, arguments...);
}

template<std::size_t S, auto Member, typename Name, typename... A>
VKAPI_ATTR void VKAPI_CALL direct(VkCommandBuffer commands, A... arguments) {
    if (auto* encoder = deferring<S>(commands, Name::Text()); encoder != nullptr && !encoder->Direct(Name::Text())) return;
    (slots()[S].real.*Member)(commands, arguments...);
}

template<std::size_t S, auto Member, typename Name, typename... A>
constexpr auto plainFor(void(VKAPI_PTR*)(VkCommandBuffer, A...)) {
    return &plain<S, Member, Name, A...>;
}

template<std::size_t S, auto Member, typename Name, typename... A>
constexpr auto directFor(void(VKAPI_PTR*)(VkCommandBuffer, A...)) {
    return &direct<S, Member, Name, A...>;
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
    if (auto* encoder = deferring<S>(commands, "cmdSetViewport")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            functions.cmdSetViewport(target, header.first, header.count, arrayAt<VkViewport>(at, header.count));
        }, RangeHeader{first, count}, std::span(viewports, count));
        return;
    }
    slots()[S].real.cmdSetViewport(commands, first, count, viewports);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL setScissor(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkRect2D* scissors) {
    if (auto* encoder = deferring<S>(commands, "cmdSetScissor")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            functions.cmdSetScissor(target, header.first, header.count, arrayAt<VkRect2D>(at, header.count));
        }, RangeHeader{first, count}, std::span(scissors, count));
        return;
    }
    slots()[S].real.cmdSetScissor(commands, first, count, scissors);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL setColorWriteMask(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkColorComponentFlags* masks) {
    if (auto* encoder = deferring<S>(commands, "cmdSetColorWriteMask")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            functions.cmdSetColorWriteMask(target, header.first, header.count, arrayAt<VkColorComponentFlags>(at, header.count));
        }, RangeHeader{first, count}, std::span(masks, count));
        return;
    }
    slots()[S].real.cmdSetColorWriteMask(commands, first, count, masks);
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
    if (auto* encoder = deferring<S>(commands, "cmdBindDescriptorSets")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<BindSetsHeader>(payload);
            const auto* at = payload + aligned(sizeof(BindSetsHeader));
            const auto* descriptorSets = arrayAt<VkDescriptorSet>(at, header.sets);
            const auto* dynamicOffsets = arrayAt<std::uint32_t>(at, header.offsets);
            functions.cmdBindDescriptorSets(target, header.bindPoint, header.layout, header.firstSet, header.sets, descriptorSets, header.offsets, dynamicOffsets);
        }, BindSetsHeader{bindPoint, layout, firstSet, sets, offsets}, std::span(descriptorSets, sets), std::span(dynamicOffsets, offsets));
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
    if (auto* encoder = deferring<S>(commands, "cmdPushConstants")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<PushHeader>(payload);
            functions.cmdPushConstants(target, header.layout, header.stages, header.offset, header.size, payload + aligned(sizeof(PushHeader)));
        }, PushHeader{layout, stages, offset, size}, std::span(static_cast<const std::byte*>(values), size));
        return;
    }
    slots()[S].real.cmdPushConstants(commands, layout, stages, offset, size, values);
}

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL bindVertexBuffers(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets) {
    if (auto* encoder = deferring<S>(commands, "cmdBindVertexBuffers")) {
        encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
            const auto header = unpack<RangeHeader>(payload);
            const auto* at = payload + aligned(sizeof(RangeHeader));
            const auto* buffers = arrayAt<VkBuffer>(at, header.count);
            const auto* offsets = arrayAt<VkDeviceSize>(at, header.count);
            functions.cmdBindVertexBuffers(target, header.first, header.count, buffers, offsets);
        }, RangeHeader{first, count}, std::span(buffers, count), std::span(offsets, count));
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
    if (auto* encoder = deferring<S>(commands, "cmdPipelineBarrier")) {
        if (!chained(memoryBarriers, memory) && !chained(bufferBarriers, buffers) && !chained(imageBarriers, images)) {
            encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                const auto header = unpack<BarrierHeader>(payload);
                const auto* at = payload + aligned(sizeof(BarrierHeader));
                const auto* memoryBarriers = arrayAt<VkMemoryBarrier>(at, header.memory);
                const auto* bufferBarriers = arrayAt<VkBufferMemoryBarrier>(at, header.buffers);
                const auto* imageBarriers = arrayAt<VkImageMemoryBarrier>(at, header.images);
                functions.cmdPipelineBarrier(target, header.source, header.destination, header.dependencies, header.memory, memoryBarriers, header.buffers, bufferBarriers, header.images, imageBarriers);
            }, BarrierHeader{source, destination, dependencies, memory, buffers, images}, std::span(memoryBarriers, memory), std::span(bufferBarriers, buffers), std::span(imageBarriers, images));
            return;
        }
        if (!encoder->Direct("cmdPipelineBarrier")) return;
    }
    slots()[S].real.cmdPipelineBarrier(commands, source, destination, dependencies, memory, memoryBarriers, buffers, bufferBarriers, images, imageBarriers);
}

struct BeginPassHeader {
    VkRenderPassBeginInfo info;
    VkSubpassContents contents;
};

template<std::size_t S>
VKAPI_ATTR void VKAPI_CALL beginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    if (auto* encoder = deferring<S>(commands, "cmdBeginRenderPass")) {
        if (info->pNext == nullptr) {
            encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                auto header = unpack<BeginPassHeader>(payload);
                const auto* at = payload + aligned(sizeof(BeginPassHeader));
                header.info.pClearValues = arrayAt<VkClearValue>(at, header.info.clearValueCount);
                functions.cmdBeginRenderPass(target, &header.info, header.contents);
            }, BeginPassHeader{*info, contents}, std::span(info->pClearValues, info->clearValueCount));
            return;
        }
        if (!encoder->Direct("cmdBeginRenderPass")) return;
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
    if (auto* encoder = deferring<S>(commands, "cmdBeginRendering")) {
        const bool plainDepth = info->pDepthAttachment == nullptr || info->pDepthAttachment->pNext == nullptr;
        const bool plainStencil = info->pStencilAttachment == nullptr || info->pStencilAttachment->pNext == nullptr;
        if (info->pNext == nullptr && plainDepth && plainStencil && !chained(info->pColorAttachments, info->colorAttachmentCount)) {
            encoder->Emplace([](const std::byte* payload, VkCommandBuffer target, const DeviceFunctions& functions) {
                auto header = unpack<RenderingHeader>(payload);
                const auto* at = payload + aligned(sizeof(RenderingHeader));
                header.info.pColorAttachments = arrayAt<VkRenderingAttachmentInfoKHR>(at, header.info.colorAttachmentCount);
                if (header.info.pDepthAttachment != nullptr) header.info.pDepthAttachment = &header.depth;
                if (header.info.pStencilAttachment != nullptr) header.info.pStencilAttachment = &header.stencil;
                functions.cmdBeginRendering(target, &header.info);
            }, RenderingHeader{*info, info->pDepthAttachment != nullptr ? *info->pDepthAttachment : VkRenderingAttachmentInfoKHR{}, info->pStencilAttachment != nullptr ? *info->pStencilAttachment : VkRenderingAttachmentInfoKHR{}}, std::span(info->pColorAttachments, info->colorAttachmentCount));
            return;
        }
        if (!encoder->Direct("cmdBeginRendering")) return;
    }
    slots()[S].real.cmdBeginRendering(commands, info);
}

template<std::size_t S>
void install(DeviceFunctions& table) {
#define APS5_PLAIN(member) \
    if (table.member != nullptr) { \
        struct Name { \
            static constexpr const char* Text() { return #member; } \
        }; \
        table.member = plainFor<S, &DeviceFunctions::member, Name>(table.member); \
    }
#define APS5_DIRECT(member) \
    if (table.member != nullptr) { \
        struct Name { \
            static constexpr const char* Text() { return #member; } \
        }; \
        table.member = directFor<S, &DeviceFunctions::member, Name>(table.member); \
    }
#define APS5_CUSTOM(member, function) \
    if (table.member != nullptr) table.member = &function<S>
    APS5_CUSTOM(cmdPipelineBarrier, pipelineBarrier);
    APS5_DIRECT(cmdCopyBuffer);
    APS5_DIRECT(cmdUpdateBuffer);
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
    APS5_CUSTOM(cmdSetColorWriteMask, setColorWriteMask);
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
    APS5_DIRECT(cmdCopyBufferToImage);
    APS5_DIRECT(cmdCopyImageToBuffer);
    APS5_DIRECT(cmdClearColorImage);
#undef APS5_PLAIN
#undef APS5_DIRECT
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

CommandEncoder::CommandEncoder(VkDevice device, DeviceFunctions& table, std::size_t slot) : table(table), real(table), device(device), slot(slot), profiled(SiteProfileRequested()), printedAt(std::chrono::steady_clock::now()) {
    static_cast<void>(HandoffSpinWindow());
    static_cast<void>(EncoderIdleSpin());
    try {
        for (; chunkCount < PooledChunks; ++chunkCount) {
            auto chunk = std::make_unique<Chunk>();
            chunk->bytes.resize(ChunkBytes);
            chunk->next = spare;
            spare = chunk.release();
        }
    } catch (...) {
        for (auto* chunk = spare; chunk != nullptr;) delete std::exchange(chunk, chunk->next);
        throw;
    }
    worker = std::thread([this] { run(); });
    std::lock_guard lock(slotMutex());
    slots()[slot].real = real;
    slots()[slot].device = device;
    installAt(slot, table, std::make_index_sequence<Slots>{});
    slots()[slot].encoder.store(this, std::memory_order_release);
}

CommandEncoder::~CommandEncoder() {
    Quiesce(WaitSite{Wait::Teardown, nullptr, 0});
    stopping.store(true, std::memory_order_seq_cst);
    posted.fetch_add(1, std::memory_order_seq_cst);
    posted.notify_all();
    worker.join();
    if (profiled) printSites(true);
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

void CommandEncoder::Open(VkCommandBuffer commands, Recording& recording, std::source_location where) {
    if (recent.load(std::memory_order_relaxed) != commands) Quiesce(WaitSite::At(Wait::Open, where));
    windows.push_back({commands, &recording});
    owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
    this->recording = &recording;
    recent.store(commands, std::memory_order_relaxed);
    target.store(commands, std::memory_order_release);
}

void CommandEncoder::RefuseForeignProducer() {
    throw std::logic_error("AGC graphics: another thread used the encoder while an encoding window was open");
}

void CommandEncoder::Close(std::source_location where) {
    const bool flushed = post();
    const auto closed = windows.back().commands;
    windows.pop_back();
    if (windows.empty()) {
        target.store(VK_NULL_HANDLE, std::memory_order_relaxed);
        recording = nullptr;
        return;
    }
    if (windows.back().commands != closed) await(WaitSite::At(Wait::Close, where), flushed);
    recording = windows.back().recording;
    recent.store(windows.back().commands, std::memory_order_relaxed);
    target.store(windows.back().commands, std::memory_order_relaxed);
}

std::byte* CommandEncoder::reserve(Run run, std::size_t bytes) {
    const auto need = RecordHeader + aligned(bytes);
    if (filling != nullptr && (filling->recording != recording || filling->used + need > filling->bytes.size())) post();
    if (filling == nullptr) {
        for (;;) {
            {
                std::lock_guard lock(queueMutex);
                if (spare != nullptr) filling = std::exchange(spare, spare->next);
            }
            if (filling != nullptr || chunkCount < PooledChunks || executed.load(std::memory_order_acquire) >= posted.load(std::memory_order_acquire)) break;
            awaitSpare();
        }
        if (filling == nullptr) {
            filling = new Chunk;
            ++chunkCount;
        }
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
    reserved = need;
    return at + RecordHeader;
}

void CommandEncoder::publish() noexcept {
    filling->used += std::exchange(reserved, 0);
    ++filling->records;
}

void CommandEncoder::refuseEncoderThread() const noexcept {
    if (std::this_thread::get_id() != worker.get_id()) return;
    std::fputs("AGC graphics: the command encoder was asked to wait for its own thread (a replayed command reached a settle); the wait could never return\n", stderr);
    std::fflush(stderr);
    std::abort();
}

bool CommandEncoder::post() noexcept {
    if (filling == nullptr || filling->records == 0) return false;
    commandCount.fetch_add(filling->records, std::memory_order_relaxed);
    const auto sequence = postCount.fetch_add(1, std::memory_order_relaxed);
    const bool timed = (sequence % PostSampleEvery) == 0;
    const auto start = timed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    {
        std::lock_guard lock(queueMutex);
        auto* chunk = std::exchange(filling, nullptr);
        (tail != nullptr ? tail->next : head) = chunk;
        tail = chunk;
    }
    posted.fetch_add(1, std::memory_order_seq_cst);
    if (sleeping.load(std::memory_order_seq_cst)) posted.notify_one();
    if (timed) postNs.fetch_add(PostSampleEvery * static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
    return true;
}

std::uint64_t CommandEncoder::handoffNs() const noexcept {
    return waitNs.load(std::memory_order_relaxed) + postNs.load(std::memory_order_relaxed);
}

bool CommandEncoder::decline(std::source_location where) noexcept {
    inlinedCount.store(inlinedCount.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    if (executed.load(std::memory_order_acquire) < posted.load(std::memory_order_acquire) || filling != nullptr) Quiesce(WaitSite::At(Wait::Open, where));
    return false;
}

bool CommandEncoder::Admit(Handoff handoff, std::source_location where) {
    if (handoff == Handoff::Never) {
        policy = HandoffPolicy{};
        return decline(where);
    }
    if (handoff == Handoff::Always) {
        policy = HandoffPolicy{};
        return true;
    }
    if (policy.Next([this] { return Measured{encodeNs.load(std::memory_order_relaxed), handoffNs()}; })) return true;
    return decline(where);
}

void CommandEncoder::Quiesce(WaitSite site) noexcept {
    const bool flushed = post();
    await(site, flushed);
}

bool CommandEncoder::Direct(const char* command) noexcept {
    Quiesce(WaitSite{Wait::Direct, command, 0});
    return recording == nullptr || !recording->Failed();
}

void CommandEncoder::Settle(WaitSite site) noexcept {
    await(site, false);
}

void CommandEncoder::await(const WaitSite& site, bool flushed) noexcept {
    refuseEncoderThread();
    const auto through = posted.load(std::memory_order_acquire);
    if (executed.load(std::memory_order_acquire) >= through) {
        if (flushed) note(site, 0, false, true);
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    if (!SpinUntil(HandoffSpinWindow(), [&] { return executed.load(std::memory_order_acquire) >= through; })) {
        for (auto seen = executed.load(std::memory_order_acquire); seen < through; seen = executed.load(std::memory_order_acquire)) executed.wait(seen, std::memory_order_acquire);
    }
    const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    waitCount.fetch_add(1, std::memory_order_relaxed);
    waitNs.fetch_add(ns, std::memory_order_relaxed);
    note(site, ns, true, flushed);
}

void CommandEncoder::note(const WaitSite& site, std::uint64_t ns, bool waited, bool flushed) noexcept {
    if (!profiled) return;
    std::unique_lock lock(siteMutex);
    auto* entry = std::find_if(sites.begin(), sites.begin() + siteCount, [&](const SiteWaits& tracked) { return sameSite(tracked.site, site); });
    if (entry == sites.begin() + siteCount) {
        if (siteCount + 1 < sites.size()) {
            ++siteCount;
            entry->site = site;
        } else {
            entry = &sites.back();
            entry->site = WaitSite{};
        }
    }
    entry->waits += waited ? 1 : 0;
    entry->waitNs += ns;
    entry->flushes += flushed ? 1 : 0;
    const auto now = std::chrono::steady_clock::now();
    if (now - printedAt < SitePrintInterval) return;
    printedAt = now;
    lock.unlock();
    printSites(false);
}

void CommandEncoder::printSites(bool final) noexcept {
    const auto totals = Counts();
    try {
        AgcDriver::ProfilePrint_nid_no_patch("[encoder] %s: %llu chunks, %llu commands, %.1f ms recording, %llu parks; %llu waits, %.1f ms; waits by site (waits/ms/flushes):%s\n", final ? "at teardown" : "so far", static_cast<unsigned long long>(totals.posts), static_cast<unsigned long long>(totals.commands), static_cast<double>(totals.encodeNs) / 1e6, static_cast<unsigned long long>(totals.parks), static_cast<unsigned long long>(totals.waits), static_cast<double>(totals.waitNs) / 1e6, Report(WaitsBySite(), PrintedSites).c_str());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[encoder] the wait-site profile could not be printed: %s\n", error.what());
    }
}

void CommandEncoder::awaitSpare() noexcept {
    refuseEncoderThread();
    const auto start = std::chrono::steady_clock::now();
    const auto seen = executed.load(std::memory_order_acquire);
    {
        std::lock_guard lock(queueMutex);
        if (spare != nullptr) return;
    }
    executed.wait(seen, std::memory_order_acquire);
    const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    waitCount.fetch_add(1, std::memory_order_relaxed);
    waitNs.fetch_add(ns, std::memory_order_relaxed);
    note(WaitSite{Wait::ChunkPool, nullptr, 0}, ns, true, false);
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
            if (!SpinUntil(EncoderIdleSpin(), [&] { return posted.load(std::memory_order_acquire) != taken; })) {
                parkCount.fetch_add(1, std::memory_order_relaxed);
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
    return {postCount.load(std::memory_order_relaxed), commandCount.load(std::memory_order_relaxed), waitCount.load(std::memory_order_relaxed), waitNs.load(std::memory_order_relaxed), encodeNs.load(std::memory_order_relaxed), parkCount.load(std::memory_order_relaxed), postNs.load(std::memory_order_relaxed), inlinedCount.load(std::memory_order_relaxed)};
}

std::vector<CommandEncoder::SiteWaits> CommandEncoder::WaitsBySite() const {
    std::vector<SiteWaits> result;
    {
        std::lock_guard lock(siteMutex);
        result.assign(sites.begin(), sites.begin() + siteCount);
        if (sites.back().waits != 0 || sites.back().flushes != 0) result.push_back(sites.back());
    }
    std::stable_sort(result.begin(), result.end(), [](const SiteWaits& left, const SiteWaits& right) { return left.waitNs != right.waitNs ? left.waitNs > right.waitNs : left.flushes > right.flushes; });
    return result;
}

std::string CommandEncoder::Report(const std::vector<SiteWaits>& sites, std::size_t limit) {
    std::string text;
    for (std::size_t index = 0; index < sites.size() && index < limit; ++index) {
        const auto& entry = sites[index];
        char numbers[96];
        std::snprintf(numbers, sizeof(numbers), " %llu/%.2f/%llu", static_cast<unsigned long long>(entry.waits), static_cast<double>(entry.waitNs) / 1e6, static_cast<unsigned long long>(entry.flushes));
        text += (index == 0 ? " " : ", ") + Describe(entry.site) + numbers;
    }
    if (sites.size() > limit) text += ", " + std::to_string(sites.size() - limit) + " more sites";
    return text.empty() ? " none" : text;
}

std::string CommandEncoder::Describe(const WaitSite& site) {
    static constexpr std::array<const char*, 11> kinds{"quiesce", "settle", "open", "close", "lookup", "direct", "behind", "chunk pool", "teardown", "unwind", "other sites"};
    std::string text = kinds[static_cast<std::size_t>(site.kind)];
    if (site.detail == nullptr) return text;
    text += ' ';
    text += site.line != 0 ? shortFunction(site.detail) : std::string_view(site.detail);
    if (site.line != 0) text += ':' + std::to_string(site.line);
    return text;
}

void CommandEncoder::SetDelayForTesting(std::function<void()> delay) {
    testDelay() = std::move(delay);
}

void BeforeCommandLookup(VkDevice device, std::source_location where) {
    const auto site = CommandEncoder::WaitSite::At(CommandEncoder::Wait::Lookup, where);
    for (auto& state : slots()) {
        auto* encoder = state.encoder.load(std::memory_order_acquire);
        if (encoder == nullptr || state.device != device) continue;
        if (encoder->OpenOnThisThread()) encoder->Quiesce(site);
        else encoder->Settle(site);
    }
}

}
