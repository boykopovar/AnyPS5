#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include <barrier>

namespace {

using AgcDriver::FrameCaptureWriter;

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
#ifdef _WIN32
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
    writer.WriteRange(address(bytes), Size, FrameRangeKind::Watched, PAGE_READWRITE, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteRange(address(cpu), FrameCaptureWriter::BlockBytes, FrameRangeKind::Unwatched, PAGE_READWRITE, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    check(writer.WriteSnapshot(address(bytes), expected) == FrameCaptureWriter::BlockBytes, "zero snapshot block was not elided");
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(std::equal(expected.begin(), expected.end(), bytes), "reused GPU memory did not restore zero and nonzero snapshot blocks");
    check(skipped.bytes == FrameCaptureWriter::BlockBytes, "unexpected skipped memory");
    check(std::all_of(cpu, cpu + FrameCaptureWriter::BlockBytes, [](std::byte value) { return value == std::byte{0x55}; }), "reused CPU memory was overwritten");
#endif
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
#ifdef _WIN32
    if (host) {
        auto* memory = static_cast<std::byte*>(VirtualAlloc(nullptr, CodeBytes + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
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
