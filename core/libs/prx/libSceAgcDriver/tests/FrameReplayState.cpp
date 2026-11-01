#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <barrier>

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

void testShaderCode(const std::filesystem::path& directory, bool host) {
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
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, CodeBytes + 4096, true, true, true);
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
    recorder->FrameBoundary();
    std::vector<std::byte> expected(CodeBytes);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<std::byte>(i * 31 + 17);
    std::memcpy(code, expected.data(), expected.size());
    recorder->RegisterShader(shader);
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    std::vector<std::byte> captured(CodeBytes);
    const auto apply = [&](const FrameMemoryChunk& chunk) {
        const auto begin = std::max(address(code), chunk.address);
        const auto end = std::min(address(code) + CodeBytes, chunk.address + chunk.bytes);
        if (begin >= end) return;
        const auto data = reader.Read(chunk);
        std::memcpy(captured.data() + begin - address(code), data.data() + begin - chunk.address, end - begin);
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
    check(registered && captured == expected, "the complete separate shader code was not captured before registration");
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected a test name and capture directory");
        const std::string mode = argv[1];
        const std::filesystem::path directory = argv[2];
        if (mode == "order") testMemoryOrder(directory);
        else if (mode == "zero") testReusedMemory(directory);
        else if (mode == "shared_page") testSharedPage(directory);
        else if (mode == "host_neighbors") testHostNeighbors(directory);
        else if (mode == "unwatched") testUnwatched();
        else if (mode == "once") testPagesCollectedOnce(directory);
        else if (mode == "pending_result") testPendingResultRead(directory);
        else if (mode == "partly_covered") testPartlyCoveredBlock(directory);
        else if (mode == "late_clear") testLateClear(directory);
        else if (mode == "tokens") testEopTokens();
        else if (mode == "blocking") testBlockingSubmission(directory);
        else if (mode == "shader" || mode == "shader_host") testShaderCode(directory, mode == "shader_host");
        else throw std::runtime_error("unknown test");
        AgcDriverShutdown_nid_postfix();
        std::puts("frame replay state test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
