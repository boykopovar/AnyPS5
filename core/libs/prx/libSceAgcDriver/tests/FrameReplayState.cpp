#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
        else if (mode == "shared_page") testSharedPage(directory);
        else if (mode == "host_neighbors") testHostNeighbors(directory);
        else if (mode == "unwatched") testUnwatched();
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
