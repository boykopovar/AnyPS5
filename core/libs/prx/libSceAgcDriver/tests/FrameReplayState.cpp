#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <limits>
#include <string_view>
#include <barrier>
#include <new>

namespace {

using AgcDriver::FrameCaptureWriter;

constexpr std::uint32_t ReadWrite = 0x04;

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

std::byte* allocate(std::size_t size, bool gpu = true) {
    auto* bytes = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(size, 65536));
    check(bytes != nullptr, "guest allocation failed");
    GuestArena::GuestArenaCommit_nid_postfix(bytes, size, 0x04, size);
#ifdef __linux__
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(bytes, size);
#endif
    GuestAllocations::Mutation mutation;
    mutation.Add(bytes, size, true, true, gpu);
    return bytes;
}

void collectGroups(Replay& replay) {
    for (const auto& chunk : replay.reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) replay.groups[chunk.group].push_back(&chunk);
    }
}

FrameCall memoryCall(std::uint64_t group, std::uint64_t thread) {
    FrameCall call;
    call.kind = FrameCallKind::Memory;
    call.values = {group};
    call.thread = thread;
    return call;
}

void testMemoryOrder(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 64u * 1024u * 1024u;
    auto* bytes = allocate(Size);
    const std::vector<std::byte> first(Size, std::byte{0x11});
    const std::vector<std::byte> second(4096, std::byte{0x22});
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(bytes), first);
    writer.WriteDelta(2, 0, address(bytes + Size - second.size()), second);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    const std::array calls{memoryCall(1, 1), memoryCall(2, 2)};
    replay.runWindow(calls);
    check(std::all_of(bytes, bytes + Size - second.size(), [](std::byte value) { return value == std::byte{0x11}; }), "first delta was not restored");
    check(std::equal(second.begin(), second.end(), bytes + Size - second.size()), "an earlier delta overwrote the later thread's delta");
}

void testReusedMemory(const std::filesystem::path& directory) {
#if defined(_WIN32) || defined(__linux__)
    constexpr std::size_t Size = 2 * FrameCaptureWriter::BlockBytes;
    auto* bytes = allocate(Size);
    auto* cpu = allocate(FrameCaptureWriter::BlockBytes, false);
    std::fill_n(bytes, Size, std::byte{0x7f});
    std::fill_n(cpu, FrameCaptureWriter::BlockBytes, std::byte{0x55});
    std::vector<std::byte> expected(Size);
    std::fill(expected.begin() + FrameCaptureWriter::BlockBytes, expected.end(), std::byte{0x22});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(bytes), Size, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteRange(address(cpu), FrameCaptureWriter::BlockBytes, FrameRangeKind::Unwatched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    check(writer.WriteSnapshot(address(bytes), expected) == FrameCaptureWriter::BlockBytes, "zero snapshot block was not elided");
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(std::equal(expected.begin(), expected.end(), bytes), "reused GPU memory did not restore zero and nonzero snapshot blocks");
    check(skipped.bytes == FrameCaptureWriter::BlockBytes, "unexpected skipped memory");
    check(std::all_of(cpu, cpu + FrameCaptureWriter::BlockBytes, [](std::byte value) { return value == std::byte{0x55}; }), "reused CPU memory was overwritten");
#endif
}

void testSharedPage(const std::filesystem::path& directory) {
#ifdef __linux__
    constexpr std::size_t Page = 65536;
    auto* page = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Page, Page));
    check(page != nullptr, "guest reservation failed");
    GuestArena::GuestArenaRelease_nid_postfix(page, Page);
    const std::vector<std::byte> gpu(0x20, std::byte{0x3c});
    const std::vector<std::byte> cpu(0x40, std::byte{0x5a});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(page + 0x20), gpu.size(), FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteRange(address(page + 0x40), cpu.size(), FrameRangeKind::Unwatched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteSnapshot(address(page + 0x20), gpu);
    writer.WriteSnapshot(address(page + 0x40), cpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "a range sharing a page with an earlier one was skipped");
    check(std::equal(gpu.begin(), gpu.end(), page + 0x20) && std::equal(cpu.begin(), cpu.end(), page + 0x40), "ranges sharing a page were not restored");
#else
    static_cast<void>(directory);
#endif
}

void testHostNeighbors(const std::filesystem::path& directory) {
#ifdef __linux__
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapped = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(mapped != MAP_FAILED, "host allocation failed");
    auto* occupied = static_cast<std::byte*>(mapped);
    auto* available = occupied + page;
    check(munmap(available, page) == 0, "host page release failed");
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(occupied), page, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteRange(address(available), page, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == page, "an occupied CPU host range was not the only range skipped");
#else
    static_cast<void>(directory);
#endif
}

void testUnwatched() {
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    try {
        recorder->FrameBoundary();
    } catch (const std::runtime_error& error) {
        check(std::string_view(error.what()).find("write watch is unavailable") != std::string_view::npos, "a capture without the write watch failed for another reason");
        return;
    }
    check(false, "a capture started without the write watch");
}

std::atomic<std::uint32_t*> pendingResult{nullptr};

void storePendingResult(std::uint64_t begin, std::size_t bytes) {
    auto* const result = pendingResult.load();
    if (result == nullptr || begin >= address(result) + sizeof(std::uint32_t) || address(result) >= begin + bytes) return;
    *result = 1;
    pendingResult = nullptr;
}

std::uint64_t groupBeforeSuspend(const FrameCaptureReader& reader, std::size_t index) {
    const FrameCall* previous = nullptr;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::Suspend) {
            if (index == 0) return previous != nullptr && previous->kind == FrameCallKind::Memory ? previous->values.at(0) : 0;
            --index;
        }
        previous = &call;
    }
    return 0;
}

std::vector<std::uint64_t> deltaGroupsHolding(const FrameCaptureReader& reader, const std::byte* page) {
    std::vector<std::uint64_t> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address >= address(page) + 4096 || address(page) >= chunk.address + chunk.bytes) continue;
        if (std::find(groups.begin(), groups.end(), chunk.group) == groups.end()) groups.push_back(chunk.group);
    }
    return groups;
}

void testPagesCollectedOnce(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* x = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* y = bytes + 4 * FrameCaptureWriter::BlockBytes;
    auto* shader = reinterpret_cast<Shader*>(x + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    std::fill_n(x, Page, std::byte{0x31});
    recorder->Suspend([] {});
    std::fill_n(x + FrameCaptureWriter::BlockBytes, Page, std::byte{0x4d});
    recorder->RegisterShader(shader);
    recorder->RegisterShader(shader);
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* lateSource = late + 2 * FrameCaptureWriter::BlockBytes;
    std::fill_n(lateSource, Page, std::byte{0x57});
    std::fill_n(y, Page, std::byte{0x65});
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto first = groupBeforeSuspend(reader, 0);
    const auto second = groupBeforeSuspend(reader, 1);
    check(first != 0 && second != 0, "the CPU writes before the first two suspend points are not in memory deltas");
    std::string failures;
    const auto expectOnce = [&](const std::byte* page, std::uint64_t group, const char* name) {
        const auto groups = deltaGroupsHolding(reader, page);
        if (groups == std::vector<std::uint64_t>{group}) return;
        failures += std::string(failures.empty() ? "" : "; ") + name + (groups.empty() ? " is in no memory delta" : " is in memory delta groups");
        for (const auto held : groups) failures += " " + std::to_string(held);
        failures += ", not only in group " + std::to_string(group);
    };
    expectOnce(x, first, "the page written before the first suspend point");
    expectOnce(y, second, "the page written before the second suspend point");
    expectOnce(lateSource, second, "the written page of the range committed during the capture");
    const auto& calls = reader.Calls();
    const auto registration = std::find_if(calls.begin(), calls.end(), [](const FrameCall& call) { return call.kind == FrameCallKind::RegisterShader; });
    if (registration == calls.end() || registration + 1 == calls.end() || (registration + 1)->kind != FrameCallKind::RegisterShader) failures += std::string(failures.empty() ? "" : "; ") + "the second registration of the shader collected its blocks again";
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testPendingResultRead(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* result = reinterpret_cast<std::uint32_t*>(page + Page - sizeof(std::uint32_t));
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    struct FlushHookReset {
        ~FlushHookReset() {
            AgcDriver::GuestMemory::SetFlushHook(nullptr);
            pendingResult = nullptr;
        }
    } reset;
    AgcDriver::GuestMemory::SetFlushHook(&storePendingResult);
    std::fill_n(page, Page - sizeof(std::uint32_t), std::byte{0x3c});
    pendingResult = result;
    recorder->Suspend([] {});
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* latePage = late + 2 * FrameCaptureWriter::BlockBytes;
    auto* lateResult = reinterpret_cast<std::uint32_t*>(latePage + Page - sizeof(std::uint32_t));
    std::fill_n(latePage, Page - sizeof(std::uint32_t), std::byte{0x3c});
    pendingResult = lateResult;
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    const FrameCaptureReader reader(directory);
    const auto first = groupBeforeSuspend(reader, 0);
    const auto second = groupBeforeSuspend(reader, 1);
    check(first != 0 && second != 0, "the CPU writes before the suspend points are not in memory deltas");
    std::string failures;
    const auto expectStored = [&](const std::uint32_t* word, std::uint64_t group, const char* name) {
        std::optional<std::uint32_t> held;
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind != FrameMemoryKind::Delta || chunk.group != group || chunk.address > address(word) || address(word) + sizeof(std::uint32_t) > chunk.address + chunk.bytes) continue;
            const auto data = reader.Read(chunk);
            std::uint32_t value = 0;
            std::memcpy(&value, data.data() + (address(word) - chunk.address), sizeof(value));
            held = value;
        }
        if (held == 1u) return;
        failures += std::string(failures.empty() ? "" : "; ") + name + (held ? " read the page before the result pending over it was stored: it holds " + std::to_string(*held) + ", not 1" : std::string(" does not hold the page the CPU wrote"));
    };
    expectStored(result, first, "the memory delta before the first suspend point");
    expectStored(lateResult, second, "the memory delta that added the range committed during the capture");
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testPartlyCoveredBlock(const std::filesystem::path& directory) {
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(4 * Page);
    std::fill_n(bytes, 4 * Page, std::byte{0x2b});
    auto* shader = reinterpret_cast<Shader*>(bytes + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->RegisterShader(shader);
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto groups = deltaGroupsHolding(reader, bytes);
    if (groups.empty()) return;
    std::string failure = "the page written before the capture, in a shader block the captured ranges cover only in part, is in memory delta groups";
    for (const auto held : groups) failure += " " + std::to_string(held);
    throw std::runtime_error(failure);
}

void testLateClear(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* x = late + 2 * FrameCaptureWriter::BlockBytes;
    auto* shader = reinterpret_cast<Shader*>(x + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* neighbor = x + FrameCaptureWriter::BlockBytes;
    std::fill_n(neighbor, Page, std::byte{0x5a});
    recorder->RegisterShader(shader);
    std::fill_n(neighbor, Page, std::byte{0});
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    std::uint64_t holding = 0;
    std::uint64_t lastGroup = 0;
    std::byte last{0};
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address > address(neighbor) || address(neighbor) + Page > chunk.address + chunk.bytes) continue;
        ++holding;
        if (chunk.group < lastGroup) continue;
        lastGroup = chunk.group;
        last = reader.Read(chunk)[address(neighbor) - chunk.address];
    }
    check(holding != 0, "the neighbour of the registered shader is in no memory delta");
    if (last != std::byte{0}) throw std::runtime_error("the last memory delta holding the neighbour of the registered shader (group " + std::to_string(lastGroup) + " of " + std::to_string(holding) + " holding it) has byte " + std::to_string(static_cast<int>(last)) + ", not the 0 the CPU stored after the registration");
}

std::byte lastDeltaByte(const FrameCaptureReader& reader, const std::byte* at) {
    std::uint64_t lastGroup = 0;
    std::byte last{0};
    bool held = false;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address > address(at) || address(at) >= chunk.address + chunk.bytes) continue;
        held = true;
        if (chunk.group < lastGroup) continue;
        lastGroup = chunk.group;
        last = reader.Read(chunk)[address(at) - chunk.address];
    }
    check(held, "the written byte is in no memory delta");
    return last;
}

void testStoreBetweenWalks(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    std::fill_n(page, 16, std::byte{0x31});
    recorder->Suspend([] {});
    AgcDriver::GuestMemory::StoreOwnBytes(address(page), 16, [&] {
        std::fill_n(page, 16, std::byte{0x42});
        page[Page - 1] = std::byte{0x6b};
    });
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto held = lastDeltaByte(reader, page + Page - 1);
    if (held != std::byte{0x6b}) throw std::runtime_error("the last memory delta holding the page has byte " + std::to_string(static_cast<int>(held)) + ", not the 107 the CPU stored between the walks of a driver store");
}

#ifdef _WIN32
bool consumeWrites(std::uintptr_t begin, std::size_t bytes, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
    std::vector<void*> pages(256);
    auto count = pages.size();
    if (!GuestArena::GuestArenaCollectWrites_nid_postfix(begin, bytes, pages.data(), &count, true)) return false;
    for (std::size_t index = 0; index < count; ++index) {
        const auto page = reinterpret_cast<std::uintptr_t>(pages[index]);
        written(context, page, page + 4096);
    }
    return true;
}

void testFreshNote(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* block = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->Suspend([] {});
    struct FreshHookReset {
        ~FreshHookReset() { AgcDriver::GuestMemory::SetFreshCollectHook(nullptr); }
    } reset;
    AgcDriver::GuestMemory::SetFreshCollectHook(&consumeWrites);
    *(block + Page) = std::byte{0x6b};
    AgcDriver::GuestMemory::MarkWritten(address(block + 3 * Page), 16);
    AgcDriver::GuestMemory::SetFreshCollectHook(nullptr);
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto held = lastDeltaByte(reader, block + Page);
    if (held != std::byte{0x6b}) throw std::runtime_error("the last memory delta holding the page has byte " + std::to_string(static_cast<int>(held)) + ", not the 107 the CPU stored before a fresh collect noted the page");
}
#endif

std::uintptr_t mockHandles = 1;
std::map<VkFence, bool> mockFences;

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mockHandles++);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(mockHandles++);
    mockFences[*fence] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    mockFences.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    return mockFences.at(fence) ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    for (std::uint32_t index = 0; index < count; ++index) mockFences.at(fences[index]) = true;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    for (std::uint32_t index = 0; index < count; ++index) mockFences.at(fences[index]) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockDeviceWaitIdle(VkDevice) {
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(mockDeviceWaitIdle)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
    };
    const auto found = table.find(name);
    return found == table.end() ? nullptr : found->second;
}

AgcDriver::Graphics::Context mockContext() {
    AgcDriver::Graphics::Context context{};
    context.device = reinterpret_cast<VkDevice>(mockHandles++);
    context.queue = reinterpret_cast<VkQueue>(mockHandles++);
    context.deviceProc = mockProc;
    return context;
}

constexpr std::size_t FilledBytes = 4096;
std::atomic<std::byte*> pendingFill{nullptr};
std::atomic<int> landedFills{0};

void landPendingFill(std::uint64_t begin, std::size_t bytes) {
    auto* const fill = pendingFill.load();
    if (fill == nullptr || begin >= address(fill) + FilledBytes || address(fill) >= begin + bytes) return;
    std::fill_n(fill, FilledBytes, std::byte{0x5a});
    pendingFill = nullptr;
    ++landedFills;
}

void testGpuWriteBeforeDelta(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    std::fill_n(bytes, FrameCaptureWriter::BlockBytes, std::byte{0x11});
    auto* page = bytes + FilledBytes;
    const std::vector<std::byte> cleared(FilledBytes);
    {
        FrameCaptureWriter writer(directory);
        writer.WriteDelta(1, 0, address(page), cleared);
        writer.Finish("{}");
    }
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    collectGroups(replay);
    struct ActiveRecorder {
        std::unique_ptr<AgcDriver::Graphics::Recorder> recorder;
        ~ActiveRecorder() {
            AgcDriver::GuestMemory::SetFlushHook(nullptr);
            pendingFill = nullptr;
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            recorder.reset();
        }
    } active{std::make_unique<AgcDriver::Graphics::Recorder>(mockContext())};
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        active.recorder->Activate();
        active.recorder->NotePendingWrite(address(page), FilledBytes);
    }
    AgcDriver::GuestMemory::SetFlushHook(&landPendingFill);
    pendingFill = page;
    check(AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(address(page), FilledBytes), "the GPU write is not pending in the driver's write snapshot");
    replay.execute(memoryCall(1, 1));
    std::vector<std::byte> replayed(FilledBytes);
    AgcDriver::GuestMemory::ReadCommitted(address(page), replayed);
    check(landedFills.load() == 1, "the GPU write pending over the page never landed, so the test did not exercise the replay's order");
    const auto stale = std::find_if(replayed.begin(), replayed.end(), [](std::byte value) { return value != std::byte{0}; });
    if (stale != replayed.end()) throw std::runtime_error("the replay restored a memory delta while a GPU write over its range was still pending, and the next read landed that write over it: the page holds byte " + std::to_string(static_cast<int>(*stale)) + ", not the 0 the capture recorded after the write");
}

void testVersionOneRefused(const std::filesystem::path& directory) {
    constexpr std::size_t Size = FrameCaptureWriter::BlockBytes;
    constexpr std::string_view VersionOne = "aps5-frame-calls 1\n";
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0x55});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    {
        FrameCaptureWriter writer(directory);
        writer.WriteRange(address(bytes), Size, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
        writer.WriteSnapshot(address(bytes), std::vector<std::byte>(Size, std::byte{0x7f}));
        writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    }
    {
        std::fstream calls(directory / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::in | std::ios::out);
        calls.write(VersionOne.data(), static_cast<std::streamsize>(VersionOne.size()));
        check(calls.good(), "cannot write the calls format 1 magic");
    }
    const FrameCaptureReader reader(directory);
    std::string refusal;
    try {
        restoreMemory(reader);
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    check(!refusal.empty(), "the replay restored a calls format 1 capture, which does not record whether GPU writes had landed before its memory deltas were read");
    if (refusal.find("record the capture again") == std::string::npos) throw std::runtime_error("the calls format 1 capture was refused for another reason: " + refusal);
    check(std::all_of(bytes, bytes + Size, [](std::byte value) { return value == std::byte{0x55}; }), "the replay restored memory before refusing the calls format 1 capture");
}

void testEopTokens() {
    Frames frames;
    auto output = std::make_shared<ReplayOutput>(frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::barrier together(2);
    std::atomic<int> wrong{0};
    const auto worker = [&](int lane) {
        for (int round = 0; round < 2000; ++round) {
            const auto requested = AgcDriver::EopFlipTokenBase + round * 8 + lane * 2;
            together.arrive_and_wait();
            try {
                if (AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, requested) != requested) ++wrong;
            } catch (const std::exception&) { ++wrong; }
        }
    };
    std::thread first(worker, 0), second(worker, 1);
    first.join();
    second.join();
    check(wrong.load() == 0, "concurrent explicit EOP reservations returned the wrong token");
    const auto high = AgcDriver::EopFlipTokenBase + 20000;
    check(AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high) == high, "explicit high token was not reserved");
    check(AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high - 1) == high - 1, "explicit lower token was not reserved");
    check(AgcDriverReserveEopFlip_nid_postfix({7, -1, 1, 0}) == high + 1, "ordinary reservation reused an explicit token");
    bool rejected = false;
    try { AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "duplicate explicit token was not rejected");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

class ReservationOutput final : public AgcDriver::IVideoOutput {
public:
    explicit ReservationOutput(Frames& frames) : frames(frames) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override {
        ++reservations;
        if (failure) std::rethrow_exception(failure);
        return available ? std::make_shared<ReplayFlip>(frames) : nullptr;
    }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }
    std::exception_ptr failure;
    bool available = true;
    std::size_t reservations = 0;

private:
    Frames& frames;
};

FrameCall reservationCall(std::uint32_t token) {
    FrameCall call;
    call.kind = FrameCallKind::ReserveEopFlip;
    call.values = {7, static_cast<std::uint64_t>(-1), 1, 99, token};
    return call;
}

template <typename TException>
void expectReservationFailure(Replay& replay, const FrameCall& call) {
    const auto kept = replay.keptReservations.load();
    bool failed = false;
    try { replay.runWindow(std::span(&call, 1)); }
    catch (const TException&) { failed = true; }
    check(failed, "the replay accepted a failed EOP reservation");
    check(replay.keptReservations.load() == kept, "a failed EOP reservation was counted as kept");
}

void testRepeatedReservations(const std::filesystem::path& directory, bool driverFailure) {
    FrameCaptureWriter writer(directory);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    auto output = std::make_shared<ReservationOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    AgcDriverRegisterVideoOutput_nid_postfix(8, output);
    const auto call = reservationCall(AgcDriver::EopFlipTokenBase);
    replay.execute(call);
    expectReservationFailure<AgcDriver::EopFlipAlreadyReserved>(replay, call);
    replay.pass = 1;
    if (driverFailure) {
        AgcDriverReportFailure_nid_postfix(std::make_exception_ptr(std::runtime_error("reservation driver failure")));
        expectReservationFailure<std::runtime_error>(replay, call);
        bool failed = false;
        try { AgcDriverShutdown_nid_postfix(); }
        catch (const std::runtime_error& error) { failed = std::string_view(error.what()) == "reservation driver failure"; }
        check(failed, "the injected driver failure was not propagated at shutdown");
        return;
    }
    replay.runWindow(std::span(&call, 1));
    check(replay.keptReservations.load() == 1 && output->reservations == 1, "a compatible pending reservation was not kept without allocating again");
    for (std::size_t field = 0; field < 4; ++field) {
        auto different = call;
        ++different.values[field];
        expectReservationFailure<std::runtime_error>(replay, different);
    }
    auto missing = call;
    missing.values[0] = 9;
    expectReservationFailure<std::runtime_error>(replay, missing);
    expectReservationFailure<std::runtime_error>(replay, reservationCall(AgcDriver::EopFlipTokenBase - 1));
    expectReservationFailure<std::runtime_error>(replay, reservationCall(AgcDriver::EopFlipTokenLimit + 1));
    const auto fresh = reservationCall(AgcDriver::EopFlipTokenBase + 1);
    output->failure = std::make_exception_ptr(std::bad_alloc{});
    expectReservationFailure<std::bad_alloc>(replay, fresh);
    output->failure = std::make_exception_ptr(std::runtime_error("reservation output failure"));
    expectReservationFailure<std::runtime_error>(replay, fresh);
    output->failure = nullptr;
    output->available = false;
    expectReservationFailure<std::runtime_error>(replay, fresh);
    output->available = true;
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    auto replacement = std::make_shared<ReservationOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, replacement);
    replay.runWindow(std::span(&call, 1));
    check(replay.keptReservations.load() == 1 && replacement->reservations == 1, "a closed output's token was kept after reopening");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, replacement);
    AgcDriverUnregisterVideoOutput_nid_postfix(8, output);
}

class WaitingOutput final : public AgcDriver::IVideoOutput {
public:
    WaitingOutput(Frames& frames, const std::uint32_t* label) : frames(frames), label(label) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (*static_cast<const volatile std::uint32_t*>(label) == 0) {
            check(Clock::now() < deadline, "a blocking driver call prevented the later memory update");
            std::this_thread::yield();
        }
        return std::make_shared<ReplayFlip>(frames);
    }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }

private:
    Frames& frames;
    const std::uint32_t* label;
};

void testTraceWindow(const std::filesystem::path& directory) {
    const char* traceDirectory = std::getenv("APS5_TRACE_DIR");
    check(traceDirectory != nullptr && AgcDriver::PerformanceTrace::Configured(), "the trace window test runs without APS5_TRACE_DIR and APS5_TRACE_FRAMES");
    const auto summaryPath = std::filesystem::path(traceDirectory) / "aps5-summary.jsonl";
    std::filesystem::remove(summaryPath);
    FrameCaptureWriter writer(directory);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, AgcDriver::FlipPacketWords> words{AgcDriver::FlipPacketHeader, 7, 0xffffffffu, 1, 0, 0};
    FrameCall flip;
    flip.kind = FrameCallKind::Submit;
    flip.thread = 1;
    flip.address = address(words.data());
    flip.size = words.size();
    const std::array calls{flip, flip, flip};
    replay.runWindow(calls);
    AgcDriverWaitIdle_nid_postfix();
    check(replay.frames.Times().size() == calls.size(), "a replayed flip was lost");
    AgcDriver::PerformanceTrace::StopOutput();
    std::ifstream summary(summaryPath);
    std::string line;
    check(static_cast<bool>(std::getline(summary, line)), "replayed frames did not open and close the trace window");
    check(line.find("\"presents\":2,") != std::string::npos, "the trace window did not count each replayed flip as a present");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testBlockingSubmission(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    const std::uint32_t ready = 1;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(label), std::as_bytes(std::span(&ready, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    collectGroups(replay);
    auto output = std::make_shared<WaitingOutput>(replay.frames, label);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, AgcDriver::FlipPacketWords> words{AgcDriver::FlipPacketHeader, 7, 0xffffffffu, 1, 0, 0};
    for (const auto kind : {FrameCallKind::Submit, FrameCallKind::Flip, FrameCallKind::ReserveEopFlip}) {
        *label = 0;
        FrameCall submit;
        submit.kind = kind;
        submit.thread = 1;
        submit.address = address(words.data());
        submit.size = words.size();
        submit.values = {7, static_cast<std::uint64_t>(-1), 1, 0, AgcDriver::EopFlipTokenBase};
        const std::array calls{submit, memoryCall(1, 2)};
        replay.runWindow(calls);
        check(*label == ready && replay.unfinished == 0, "blocking submission did not complete");
    }
    check(replay.frames.Times().size() == 2, "blocking submissions lost a flip");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testShaderCode(const std::filesystem::path& directory, bool host, std::string_view mode = {}) {
    const bool prologue = mode.starts_with("shader_prologue_");
    const bool readOnly = mode.ends_with("readonly");
    const bool hostHeader = readOnly || mode.ends_with("header");
    std::byte* hostMemory = nullptr;
    auto* bytes = allocate(1u << 20);
    auto* shader = reinterpret_cast<Shader*>(bytes);
    auto* code = bytes + (1u << 19) + 256;
    constexpr std::size_t CodeBytes = 3 * FrameCaptureWriter::BlockBytes;
#if defined(_WIN32) || defined(__linux__)
    if (host) {
#ifdef _WIN32
        auto* memory = static_cast<std::byte*>(VirtualAlloc(nullptr, CodeBytes + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        void* mapped = mmap(nullptr, CodeBytes + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        auto* memory = mapped != MAP_FAILED ? static_cast<std::byte*>(mapped) : nullptr;
#endif
        check(memory != nullptr, "host shader allocation failed");
        if (!prologue) {
            GuestAllocations::Mutation mutation;
            mutation.Add(memory, CodeBytes + 4096, true, true, true);
        }
        hostMemory = memory;
        if (hostHeader) shader = reinterpret_cast<Shader*>(memory);
        code = memory + 256;
    }
#endif
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    shader->shader_size = CodeBytes;
    shader->code = code;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    if (!prologue) recorder->FrameBoundary();
    std::vector<std::byte> expected(CodeBytes);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<std::byte>(i * 31 + 17);
    std::memcpy(code, expected.data(), expected.size());
    std::vector<std::byte> expectedHeader(sizeof(Shader));
    std::memcpy(expectedHeader.data(), shader, expectedHeader.size());
    if (readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        check(VirtualProtect(hostMemory, CodeBytes + 4096, PAGE_READONLY, &previous) != 0, "cannot protect host shader pages");
#elif defined(__linux__)
        check(mprotect(hostMemory, CodeBytes + 4096, PROT_READ) == 0, "cannot protect host shader pages");
#endif
    }
    recorder->RegisterShader(shader);
    recorder->FrameBoundary();
    if (prologue) recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    std::vector<std::byte> captured(CodeBytes);
    std::vector<std::byte> capturedHeader(sizeof(Shader));
    const auto applyBytes = [&](const FrameMemoryChunk& chunk, std::uint64_t target, std::span<std::byte> bytes) {
        const auto begin = std::max(target, chunk.address);
        const auto end = std::min(target + bytes.size(), chunk.address + chunk.bytes);
        if (begin >= end) return;
        const auto data = reader.Read(chunk);
        std::memcpy(bytes.data() + begin - target, data.data() + begin - chunk.address, end - begin);
    };
    const auto apply = [&](const FrameMemoryChunk& chunk) {
        applyBytes(chunk, address(code), captured);
        if (hostHeader) applyBytes(chunk, address(shader), capturedHeader);
    };
    for (const auto& chunk : reader.Chunks()) if (chunk.kind == FrameMemoryKind::Snapshot) apply(chunk);
    bool registered = false;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::RegisterShader) {
            registered = true;
            break;
        }
        if (call.kind != FrameCallKind::Memory) continue;
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind == FrameMemoryKind::Delta && chunk.group == call.values.at(0)) apply(chunk);
        }
    }
    if (hostHeader && captured == expected) check(capturedHeader == expectedHeader, "the host shader header was not captured before registration");
    if (readOnly && captured == expected) {
        const bool protectedHeader = std::ranges::any_of(reader.Chunks(), [&](const auto& chunk) {
            return chunk.kind == FrameMemoryKind::Range && chunk.address <= address(shader) &&
                address(shader) - chunk.address < chunk.bytes && (chunk.access & AgcDriver::FrameRangeAccess::Writable) == 0;
        });
        check(protectedHeader, "the read-only host shader was recorded with write access");
    }
    check(registered && captured == expected, "the complete separate shader code was not captured before registration");
    CapturedMemory memory(reader);
    std::map<std::uint64_t, std::vector<const FrameMemoryChunk*>> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) groups[chunk.group].push_back(&chunk);
    }
    std::optional<std::vector<std::byte>> listed;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::RegisterShader) {
            listed = memory.Read(address(code), CodeBytes);
            break;
        }
        if (call.kind != FrameCallKind::Memory) continue;
        if (const auto found = groups.find(call.values.at(0)); found != groups.end()) memory.Apply(found->second);
    }
    check(listed && *listed == expected, "the offline shader listing read other code than the capture holds at the registration");
}

void writeInspectionCapture(const std::filesystem::path& directory) {
    constexpr std::uint64_t Commands = 0x1000000001ull;
    constexpr std::uint64_t Header = 0x2000000000ull;
    constexpr std::uint64_t Code = 0x2000010000ull;
    const std::array<std::uint32_t, 27> commands{
        0x00010000u, 0xc0053c00u, 0xc0064900u, 0x80000000u, 0xffff1000u,
        0xc0053c00u, 0x13u, 0x12345678u, 0x9abcdef0u, 0x10203040u, 0xf0f0f0f0u, 4u,
        0xc0064900u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
        0xc0033700u, 0x500u, 0xdeadbeefu, 0x12u, 0x1337u,
        0x00030000u, 0xc0053c00u};
    std::array<std::uint8_t, 16> code{0, 1, 2, 3, 4, 5, 6, 7, 255, 128, 13, 10, 39, 36, 38, 0};
    Shader shader{};
    shader.code = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Code));
    shader.shader_size = code.size();
    FrameCaptureWriter writer(directory);
    writer.WriteRange(Commands, 6, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Commands + 6, sizeof(commands) - 6, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Header, sizeof(shader), FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Code, code.size(), FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteSnapshot(Header, std::as_bytes(std::span(&shader, 1)));
    writer.WriteSnapshot(Code, std::as_bytes(std::span(code)));
    writer.WriteDelta(1, 0, Commands, std::as_bytes(std::span(commands)));
    const std::uint32_t reference = 0x55667788u;
    writer.WriteDelta(2, 0, Commands + 9 * sizeof(std::uint32_t), std::as_bytes(std::span(&reference, 1)));
    code[0] = 0x42;
    writer.WriteDelta(3, 0, Code, std::as_bytes(std::span(code)));
    FrameCall call;
    const auto write = [&](FrameCallKind kind, std::uint64_t location, std::uint64_t size, std::vector<std::uint64_t> values = {}) {
        call.kind = kind;
        ++call.ordinal;
        call.address = location;
        call.size = size;
        call.values = std::move(values);
        call.thread = 7;
        call.queue = 0x42;
        call.accepted = 12;
        call.completed = 9;
        writer.WriteCall(call);
    };
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::Memory, 0, 0, {1});
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::FrameStart, 0, 0);
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::Memory, 0, 0, {2});
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::Memory, 0, 0, {3});
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::Flip, 0, 0);
    write(FrameCallKind::ReserveEopFlip, 0, 0);
    write(FrameCallKind::Submit, 0x3000000000ull, 1);
    write(FrameCallKind::Submit, Commands, std::numeric_limits<std::uint64_t>::max());
    write(FrameCallKind::Submit, std::numeric_limits<std::uint64_t>::max() - 1, 1);
    write(FrameCallKind::Submit, Commands, 0);
    writer.Finish("{}");
}

void testColdPipelines() {
    const auto* inherited = std::getenv("APS5_HEADLESS_PIPELINE_CACHE");
    check(inherited != nullptr && std::string_view(inherited) == "1", "pipeline cache setting was not inherited");
    Options options;
    options.coldPipelines = true;
    configureEnvironment(options);
    check(std::getenv("APS5_HEADLESS_PIPELINE_CACHE") == nullptr, "cold replay kept the inherited pipeline cache setting");
    check(std::getenv("APS5_FRAME_CAPTURE") == nullptr, "replay kept the capture setting");
    options.coldPipelines = false;
    configureEnvironment(options);
    const auto* warm = std::getenv("APS5_HEADLESS_PIPELINE_CACHE");
    check(warm != nullptr && std::string_view(warm) == "1", "warm replay did not enable the pipeline cache");
#ifdef _WIN32
    _putenv_s("APS5_HEADLESS_PIPELINE_CACHE", "0");
#else
    setenv("APS5_HEADLESS_PIPELINE_CACHE", "0", 1);
#endif
    options.coldPipelines = true;
    configureEnvironment(options);
    check(std::getenv("APS5_HEADLESS_PIPELINE_CACHE") == nullptr, "cold replay kept a present zero pipeline cache setting");
}

std::optional<std::pair<std::uint64_t, std::byte>> lastRecorded(const FrameCaptureReader& reader, const std::byte* at) {
    std::optional<std::pair<std::uint64_t, std::byte>> last;
    for (const auto& chunk : reader.Chunks()) {
        if ((chunk.kind != FrameMemoryKind::Delta && chunk.kind != FrameMemoryKind::Snapshot) || chunk.address > address(at) || address(at) >= chunk.address + chunk.bytes) continue;
        const std::uint64_t group = chunk.kind == FrameMemoryKind::Delta ? chunk.group : 0;
        if (last && group < last->first) continue;
        last = std::pair{group, reader.Read(chunk)[address(at) - chunk.address]};
    }
    return last;
}

std::string describeRecorded(const std::optional<std::pair<std::uint64_t, std::byte>>& last) {
    if (!last) return "in no memory delta and not in the snapshot";
    const auto where = last->first == 0 ? std::string("last recorded by the snapshot") : "last recorded in memory delta group " + std::to_string(last->first);
    return where + " with byte " + std::to_string(static_cast<int>(last->second));
}

void testStoreWindow(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* label = reinterpret_cast<std::uint32_t*>(page + 0x100);
    auto* title = page + 0x800;
    auto* own = bytes + 4 * FrameCaptureWriter::BlockBytes;
    own[4 * Page + 0x123] = std::byte{0x6d};
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    AgcDriver::GuestMemory::StoreOwnBytes(address(label), sizeof(*label), [&] {
        *label = 1;
        std::thread([&] { std::fill_n(title, 64, std::byte{0x7e}); }).join();
    });
    const std::array<std::byte, 8> eight{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    AgcDriver::GuestMemory::StoreOwnBytes(address(own + Page + 0x40), eight);
    AgcDriver::GuestMemory::Write(address(own + 2 * Page + 0x80), eight, 4);
    AgcDriverStoreGuestLabel_nid_postfix(reinterpret_cast<std::uint64_t*>(own + 3 * Page + 0x10), 7);
    const std::vector<std::byte> original(Page, std::byte{0});
    auto current = original;
    current[0x10] = std::byte{0x5c};
    AgcDriver::GuestMemory::WriteChanged(address(own + 4 * Page), current, original);
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped") == std::string::npos, ("the capture stopped: " + reader.Header()).c_str());
    std::string failures;
    const auto last = lastRecorded(reader, title);
    if (!last || last->first == 0 || last->second != std::byte{0x7e}) failures = "the store another thread made in the page of a driver store during the store is " + describeRecorded(last) + ", not 126";
    const std::array<const char*, 4> stores{"StoreOwnBytes with given bytes", "Write", "AgcDriverStoreGuestLabel", "WriteChanged"};
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const auto groups = deltaGroupsHolding(reader, own + (index + 1) * Page);
        if (groups.empty()) continue;
        failures += std::string(failures.empty() ? "" : "; ") + "the page " + stores[index] + " stored into with no other store in it is in memory delta groups";
        for (const auto held : groups) failures += " " + std::to_string(held);
    }
    auto* after = bytes + 6 * FrameCaptureWriter::BlockBytes;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    const auto since = AgcDriver::GuestMemory::CollectWrittenBlocks(address(after), Page, std::numeric_limits<std::uint64_t>::max(), written);
    AgcDriver::GuestMemory::StoreOwnBytes(address(after + 0x100), 8, [&] { after[0x100] = std::byte{1}; });
    AgcDriver::GuestMemory::CollectWrittenBlocks(address(after), Page, since, written);
    if (!written.empty()) failures += std::string(failures.empty() ? "" : "; ") + "a driver store made after the capture finished was listed as a CPU write";
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testStoreWindowUnwritten(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    auto* bytes = allocate(Size);
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    const std::array<std::byte, 16> zero{};
    AgcDriver::GuestMemory::Write(address(page + 0x100), zero);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->Suspend([] {});
    auto* label = reinterpret_cast<std::uint32_t*>(page + 0x100);
    auto* title = page + 0x800;
    AgcDriver::GuestMemory::StoreOwnBytes(address(label), sizeof(*label), [&] {
        *label = 1;
        std::thread([&] { std::fill_n(title, 64, std::byte{0x7e}); }).join();
    });
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped") == std::string::npos, ("the capture stopped: " + reader.Header()).c_str());
    const auto last = lastRecorded(reader, title);
    if (!last || last->first == 0 || last->second != std::byte{0x7e}) throw std::runtime_error("the store another thread made during a driver store into a page the CPU has not written is " + describeRecorded(last) + ", not 126");
}

void testStoreWindowStopped(const std::filesystem::path& directory) {
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    std::fill_n(bytes, FrameCaptureWriter::BlockBytes, std::byte{0});
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped: less than 4 GiB free on the capture volume") != std::string::npos, "the capture did not stop when it started");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    const auto since = AgcDriver::GuestMemory::CollectWrittenBlocks(address(bytes), Page, std::numeric_limits<std::uint64_t>::max(), written);
    AgcDriver::GuestMemory::StoreOwnBytes(address(bytes + 0x100), 8, [&] { bytes[0x100] = std::byte{1}; });
    AgcDriver::GuestMemory::CollectWrittenBlocks(address(bytes), Page, since, written);
    if (!written.empty()) throw std::runtime_error("a driver store made after the capture stopped was listed as a CPU write");
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected a test name and capture directory");
        const std::string mode = argv[1];
        const std::filesystem::path directory = argv[2];
        if (mode == "inspect_capture") {
            writeInspectionCapture(directory);
            return 0;
        }
        if (mode == "order") testMemoryOrder(directory);
        else if (mode == "zero") testReusedMemory(directory);
        else if (mode.starts_with("shader_prologue_")) testShaderCode(directory, true, mode);
        else if (mode == "shared_page") testSharedPage(directory);
        else if (mode == "host_neighbors") testHostNeighbors(directory);
        else if (mode == "unwatched") testUnwatched();
        else if (mode == "once") testPagesCollectedOnce(directory);
        else if (mode == "pending_result") testPendingResultRead(directory);
        else if (mode == "partly_covered") testPartlyCoveredBlock(directory);
        else if (mode == "late_clear") testLateClear(directory);
        else if (mode == "store_between_walks") testStoreBetweenWalks(directory);
        else if (mode == "store_window") testStoreWindow(directory);
        else if (mode == "store_window_unwritten") testStoreWindowUnwritten(directory);
        else if (mode == "store_window_stopped") testStoreWindowStopped(directory);
#ifdef _WIN32
        else if (mode == "fresh_note") testFreshNote(directory);
#endif
        else if (mode == "gpu_write_before_delta") testGpuWriteBeforeDelta(directory);
        else if (mode == "calls_v1") testVersionOneRefused(directory);
        else if (mode == "trace_window") testTraceWindow(directory);
        else if (mode == "tokens") testEopTokens();
        else if (mode == "reservations" || mode == "reservation_failure") testRepeatedReservations(directory, mode == "reservation_failure");
        else if (mode == "blocking") testBlockingSubmission(directory);
        else if (mode == "environment") testColdPipelines();
        else if (mode == "shader" || mode == "shader_host") testShaderCode(directory, mode == "shader_host");
        else throw std::runtime_error("unknown test");
        if (mode != "reservation_failure") AgcDriverShutdown_nid_postfix();
        std::puts("frame replay state test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
