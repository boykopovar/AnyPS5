#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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

void rewriteCallsVersion(const std::filesystem::path& capture, std::uint32_t version) {
    static_assert(FrameCaptureWriter::CallsMagic.size() == 19);
    std::fstream file(capture / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(static_cast<std::streamoff>(FrameCaptureWriter::CallsMagic.size() - 2));
    file.put(static_cast<char>('0' + version));
    check(file.good(), "cannot rewrite the calls format of the synthetic capture");
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

void testGateSettled(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    auto* restored = reinterpret_cast<std::uint32_t*>(bytes + 8192);
    *label = 0;
    *restored = 0;
    const std::array<std::uint32_t, 7> wait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(wait.begin(), wait.end(), commands);
    const std::uint32_t stored = 7;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(restored), std::as_bytes(std::span(&stored, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.paceTimeout = std::chrono::milliseconds(300);
    options.busyTimeout = std::chrono::milliseconds(20000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    AgcDriverSubmitGraphics_nid_postfix(commands, static_cast<std::uint32_t>(wait.size()));
    const auto parked = [&] {
        std::array<AgcDriver::AwaitedWait, 64> awaited{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(awaited.data(), awaited.size());
        return std::any_of(awaited.begin(), awaited.begin() + static_cast<std::ptrdiff_t>(count), [&](const AgcDriver::AwaitedWait& slot) { return slot.address == address(label); });
    };
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!parked()) {
        check(Clock::now() < deadline, "the submission did not wait on its unwritten label");
        std::this_thread::yield();
    }
    const auto [accepted, completed] = progress();
    check(accepted == completed + 1, "the parked submission was not the only one in flight");
    auto call = memoryCall(1, 1);
    call.ordinal = 1;
    call.flags = AgcDriver::InFlightDeltaFlag;
    call.accepted = accepted;
    call.completed = completed;
    call.values = {1, accepted};
    std::string failure;
    try {
        replay.runWindow(std::span(&call, 1));
    } catch (const std::runtime_error& error) {
        failure = error.what();
    }
    const auto delta = std::atomic_ref<std::uint32_t>(*restored).load(std::memory_order_acquire);
    std::atomic_ref<std::uint32_t>(*label).store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    char blocked[64];
    std::snprintf(blocked, sizeof(blocked), "; blocked on 0x%llx (", static_cast<unsigned long long>(address(label)));
    check(failure.find("but no queue runs and the serials stopped at " + std::to_string(completed) + "/" + std::to_string(accepted) + blocked) != std::string::npos, ("the gate of a delta read in flight did not end once no queue ran while the submission it waits for stayed parked on title memory: " + failure).c_str());
    check(delta == 0, "the replay restored a delta read in flight although the submission it waits for never completed");
}

void testDepthAfterSubmits(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    *label = 0;
    const std::array<std::uint32_t, 7> wait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(wait.begin(), wait.end(), commands);
    const std::uint32_t ready = 1;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(label), std::as_bytes(std::span(&ready, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.paceTimeout = std::chrono::milliseconds(300);
    options.busyTimeout = std::chrono::milliseconds(5000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    FrameCall parked;
    parked.kind = FrameCallKind::Submit;
    parked.ordinal = 1;
    parked.thread = 1;
    parked.address = address(commands);
    parked.size = wait.size();
    auto reservation = reservationCall(AgcDriver::EopFlipTokenBase);
    reservation.ordinal = 2;
    reservation.thread = 1;
    reservation.accepted = 1;
    auto delta = memoryCall(1, 1);
    delta.ordinal = 3;
    const std::array calls{parked, reservation, delta};
    replay.runWindow(calls);
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    check(*label == ready && replay.unfinished == 0, "the parked submission did not complete once the delta restored its label");
    check(replay.depthSamples.load() == 1 && replay.depthMax.load() == 0, ("the replay sampled the in-flight depth at an EOP flip reservation, while submit_depths counts only submissions: " + std::to_string(replay.depthSamples.load()) + " samples, in_flight_max " + std::to_string(replay.depthMax.load())).c_str());
}

void testDepthOfTimedPasses(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    auto* nop = reinterpret_cast<std::uint32_t*>(bytes + 8192);
    *label = 0;
    const std::array<std::uint32_t, 7> wait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(wait.begin(), wait.end(), commands);
    const std::array<std::uint32_t, 2> empty{0xc0001000u, 0u};
    std::copy(empty.begin(), empty.end(), nop);
    const std::uint32_t ready = 1;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(label), std::as_bytes(std::span(&ready, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.paceTimeout = std::chrono::milliseconds(300);
    options.busyTimeout = std::chrono::milliseconds(5000);
    Replay replay{options, reader};
    collectGroups(replay);
    FrameCall prologue;
    prologue.kind = FrameCallKind::Submit;
    prologue.ordinal = 1;
    prologue.thread = 1;
    prologue.address = address(commands);
    prologue.size = wait.size();
    replay.execute(prologue);
    auto submit = prologue;
    submit.ordinal = 2;
    submit.address = address(nop);
    submit.size = empty.size();
    submit.accepted = 1;
    auto delta = memoryCall(1, 1);
    delta.ordinal = 3;
    const std::array window{submit, delta};
    replay.recording = true;
    replay.runWindow(window);
    check(*label == ready && replay.unfinished == 0 && replay.depthSamples.load() == 1 && replay.depthMax.load() == 1, "the first pass did not sample its submission at depth 1 behind the prologue's submission parked on a label");
    AgcDriverWaitIdle_nid_postfix();
    replay.pass = 1;
    replay.runWindow(window);
    check(replay.unfinished == 0, "the second pass did not complete");
    check(replay.depthSamples.load() == 1 && replay.depthMax.load() == 0, ("the in-flight depth of the second pass kept the first pass's sample, which warmed the caches and which frame_ms leaves out: " + std::to_string(replay.depthSamples.load()) + " samples, in_flight_max " + std::to_string(replay.depthMax.load())).c_str());
}

void testGateFailure(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    *label = 0;
    const std::array<std::uint32_t, 7> wait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(wait.begin(), wait.end(), commands);
    const std::uint32_t stored = 7;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(bytes + 8192), std::as_bytes(std::span(&stored, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.busyTimeout = std::chrono::milliseconds(3000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    AgcDriverSubmitGraphics_nid_postfix(commands, static_cast<std::uint32_t>(wait.size()));
    const auto parked = [&] {
        std::array<AgcDriver::AwaitedWait, 64> awaited{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(awaited.data(), awaited.size());
        return std::any_of(awaited.begin(), awaited.begin() + static_cast<std::ptrdiff_t>(count), [&](const AgcDriver::AwaitedWait& slot) { return slot.address == address(label); });
    };
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!parked()) {
        check(Clock::now() < deadline, "the submission did not wait on its unwritten label");
        std::this_thread::yield();
    }
    const auto [accepted, completed] = progress();
    AgcDriverReportFailure_nid_postfix(std::make_exception_ptr(std::runtime_error("gate driver failure")));
    auto call = memoryCall(1, 1);
    call.ordinal = 1;
    call.flags = AgcDriver::InFlightDeltaFlag;
    call.accepted = accepted;
    call.completed = completed;
    call.values = {1, accepted};
    std::string failure;
    try {
        replay.runWindow(std::span(&call, 1));
    } catch (const std::runtime_error& error) {
        failure = error.what();
    }
    check(failure == "gate driver failure", "a driver failure during a flagged delta's gate did not end the replay with the driver's error");
    bool rethrown = false;
    try { AgcDriverShutdown_nid_postfix(); }
    catch (const std::runtime_error& error) { rethrown = std::string_view(error.what()) == "gate driver failure"; }
    check(rethrown, "the injected driver failure was not propagated at shutdown");
}

void testRewindBreaker(const std::filesystem::path& directory, bool previous) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    *label = 0;
    const std::array<std::uint32_t, 7> rewound{0xc0005900u, 0u, 0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u};
    std::copy(rewound.begin(), rewound.end(), commands);
    auto* const control = commands + 1;
    const std::uint32_t valid = AgcDriver::Pm4::RewindValid;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(control), std::as_bytes(std::span(&valid, 1)));
    writer.Finish("{}");
    if (previous) rewriteCallsVersion(directory, AgcDriver::InFlightCallsVersion - 1);
    const FrameCaptureReader reader(directory);
    check((reader.Version() < AgcDriver::InFlightCallsVersion) == previous, "the synthetic capture is not of the calls format this mode replays");
    Options options;
    options.paceTimeout = std::chrono::milliseconds(previous ? 5000 : 300);
    options.busyTimeout = std::chrono::milliseconds(5000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    AgcDriverSubmitGraphics_nid_postfix(commands, static_cast<std::uint32_t>(rewound.size()));
    const auto parked = [&] {
        std::array<AgcDriver::AwaitedWait, 64> awaited{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(awaited.data(), awaited.size());
        return std::any_of(awaited.begin(), awaited.begin() + static_cast<std::ptrdiff_t>(count), [&](const AgcDriver::AwaitedWait& slot) { return slot.address == address(control); });
    };
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!parked()) {
        check(Clock::now() < deadline, "the submission did not park at its unreleased REWIND");
        std::this_thread::yield();
    }
    const auto [accepted, completed] = progress();
    check(accepted == completed + 1, "the REWIND submission was not the only one in flight");
    auto reservation = reservationCall(AgcDriver::EopFlipTokenBase);
    reservation.ordinal = 1;
    reservation.thread = 1;
    reservation.accepted = accepted;
    reservation.completed = accepted;
    auto delta = memoryCall(1, 1);
    delta.ordinal = 2;
    delta.flags = previous ? 0u : AgcDriver::InFlightDeltaFlag;
    delta.accepted = accepted;
    delta.completed = completed;
    const std::array calls{reservation, delta};
    replay.runWindow(calls);
    check(replay.lateReleases.load() == 0 && replay.syntheticReleases.load() == 0, "the breaker released the REWIND with a recorded value or one derived from the reference");
    if (previous) {
        check(replay.earlyReleases.load() == 1 && replay.paceTimeouts.load() == 0, "the breaker of the replay of a capture of the previous calls format did not apply the later memory delta early to release the REWIND before pacing gave up");
    } else {
        check(replay.earlyReleases.load() == 0, "the breaker released the REWIND early with the bytes of a memory delta read in flight, before that delta's record");
        check(replay.paceTimeouts.load() == 1, "the replay did not give up pacing while its queue waited on the REWIND");
    }
    check(std::atomic_ref<std::uint32_t>(*label).load() == 1 && replay.unfinished == 0, "the REWIND did not run its tail once its memory delta was restored");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testSerialDiverged(const std::filesystem::path& directory) {
    constexpr std::uint64_t Base = 40;
    const char* dump = std::getenv("APS5_DUMP_SUBMITS");
    check(std::getenv("APS5_CPU_FLIP_IN_ORDER") == nullptr && std::getenv("APS5_SYNC_FLIP") == nullptr && (dump == nullptr || *dump == '\0'), "the environment orders CPU flips or captures submissions, so the replay's CPU flip would take a serial");
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes);
    const std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
    std::copy(nop.begin(), nop.end(), commands);
    const std::uint32_t stored = 7;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(bytes + 8192), std::as_bytes(std::span(&stored, 1)));
    FrameCall start;
    start.kind = FrameCallKind::FrameStart;
    start.accepted = Base;
    start.completed = Base;
    writer.WriteCall(start);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.busyTimeout = std::chrono::milliseconds(3000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    const auto submit = [&](std::uint32_t queue, std::vector<std::uint64_t> taken) {
        FrameCall call;
        call.kind = FrameCallKind::Submit;
        call.ordinal = 1;
        call.thread = 1;
        call.queue = queue;
        call.address = address(commands);
        call.size = nop.size();
        call.accepted = Base;
        call.completed = Base;
        call.values = std::move(taken);
        return call;
    };
    const auto suspend = [&](std::vector<std::uint64_t> taken) {
        FrameCall call;
        call.kind = FrameCallKind::Suspend;
        call.ordinal = 1;
        call.thread = 1;
        call.accepted = Base;
        call.completed = Base;
        call.values = std::move(taken);
        return call;
    };
    const auto delta = [&](std::uint64_t ordinal, std::uint64_t accepted, std::uint64_t listed) {
        auto call = memoryCall(1, 1);
        call.ordinal = ordinal;
        call.flags = AgcDriver::InFlightDeltaFlag;
        call.accepted = accepted;
        call.completed = Base;
        call.values = {1, listed};
        return call;
    };
    const auto refusal = [&](std::span<const FrameCall> calls) {
        std::string error;
        try {
            replay.runWindow(calls);
        } catch (const std::runtime_error& failure) {
            error = failure.what();
        }
        AgcDriverWaitIdle_nid_postfix();
        return error;
    };
    const std::array mapped{submit(0, {Base + 1}), delta(2, Base + 1, Base + 1)};
    const auto replayed = refusal(mapped);
    check(replayed.empty(), ("the replay refused a submission recorded as taking the serial after the frame start: " + replayed).c_str());
    if (!APS5_ENABLE_TIMING_LOG) {
        FrameCall flip;
        flip.kind = FrameCallKind::Flip;
        flip.ordinal = 2;
        flip.thread = 1;
        flip.cpuFlip = true;
        flip.accepted = Base + 1;
        flip.completed = Base;
        flip.values = {7, static_cast<std::uint64_t>(-1), 1, 0, Base + 3};
        const std::array flipped{submit(0x20, {Base + 1}), flip, delta(3, Base + 3, Base + 1)};
        const auto skipped = refusal(flipped);
        check(skipped.find("took no replay serial") != std::string::npos, ("the replay ran a CPU flip recorded as taking the third serial after the frame start, after a submission that took the first, and did not refuse the delta read in flight behind it: " + skipped).c_str());
    }
    const std::array diverged{submit(0, {Base + 2}), delta(2, Base + 2, Base + 2)};
    const auto unmapped = refusal(diverged);
    check(unmapped.find("took replay serial") != std::string::npos, ("the replay ran a submission recorded as taking the second serial after the frame start, which no recorded call before it took, and did not refuse the delta read in flight behind it: " + unmapped).c_str());
    const std::array unrecorded{submit(0, {}), delta(2, Base + 1, Base + 1)};
    const auto missing = refusal(unrecorded);
    check(missing.find("has no recorded serial") != std::string::npos, ("the replay ran a submission that recorded no serial before a delta read in flight: " + missing).c_str());
    const std::array unlined{delta(1, Base + 1, Base + 1)};
    const auto offset = refusal(unlined);
    check(offset.find("but this replay had accepted") != std::string::npos, ("the replay restored a delta read in flight at a serial that no call before it in the window took: " + offset).c_str());
    const std::array listedBefore{submit(0, {Base + 1}), delta(2, Base + 1, Base)};
    const auto unlisted = refusal(listedBefore);
    check(unlisted.find("no call since the frame start recorded that serial") != std::string::npos, ("the replay restored a delta read in flight that lists a serial no call since the frame start took: " + unlisted).c_str());
    FrameCall other;
    other.kind = FrameCallKind::DisplayBuffer;
    other.ordinal = 2;
    other.thread = 2;
    const std::array threaded{submit(0, {Base + 1}), other, delta(3, Base + 1, Base + 1)};
    const auto unordered = refusal(threaded);
    check(unordered.find("after a second thread made calls") != std::string::npos, ("the replay restored a delta read in flight after a second thread's call, which it cannot order: " + unordered).c_str());
    const std::array divergedSuspend{suspend({Base + 2}), delta(2, Base + 2, Base + 2)};
    const auto unmappedSuspend = refusal(divergedSuspend);
    check(unmappedSuspend.find("took replay serial") != std::string::npos, ("the replay ran a suspend point recorded as taking the second serial after the frame start, which no recorded call before it took, and did not refuse the delta read in flight behind it: " + unmappedSuspend).c_str());
    const auto refusedBeforeRestore = [&](const std::string& name, std::span<const FrameCall> calls) {
        const auto capture = directory / name;
        {
            FrameCaptureWriter captureWriter(capture);
            captureWriter.WriteCall(start);
            for (const auto& call : calls) captureWriter.WriteCall(call);
            captureWriter.Finish("{}");
        }
        std::string error;
        try {
            Options paced;
            paced.directory = capture;
            run(paced);
        } catch (const std::runtime_error& failure) {
            error = failure.what();
        }
        return error;
    };
    const auto early = refusedBeforeRestore("unrecorded", unrecorded);
    check(early.find("has no recorded serial") != std::string::npos, ("the replay did not refuse before its restore a capture whose submission before a delta read in flight recorded no serial: " + early).c_str());
    const std::array unrecordedSuspend{suspend({}), delta(2, Base + 1, Base + 1)};
    const auto earlySuspend = refusedBeforeRestore("unsuspended", unrecordedSuspend);
    check(earlySuspend.find("has no recorded serial") != std::string::npos, ("the replay did not refuse before its restore a capture whose suspend point before a delta read in flight recorded no serial: " + earlySuspend).c_str());
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

class PresentingFlip final : public AgcDriver::IFlipRequest {
public:
    PresentingFlip(Frames& frames, const std::uint32_t* restored, std::atomic<std::uint32_t>& seen) : frames(frames), restored(restored), seen(seen) {}
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        seen.store(*static_cast<const volatile std::uint32_t*>(restored));
        frames.MarkReady();
    }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }

private:
    Frames& frames;
    const std::uint32_t* restored;
    std::atomic<std::uint32_t>& seen;
};

class PresentingOutput final : public AgcDriver::IVideoOutput {
public:
    PresentingOutput(Frames& frames, const std::uint32_t* restored) : frames(frames), restored(restored) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<PresentingFlip>(frames, restored, seen); }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }
    std::atomic<std::uint32_t> seen{~0u};

private:
    Frames& frames;
    const std::uint32_t* restored;
};

void testSerialFlipInOrder(const std::filesystem::path& directory) {
    constexpr std::uint64_t Base = 40;
    check(std::getenv("APS5_CPU_FLIP_IN_ORDER") != nullptr, "serial_flip_in_order needs APS5_CPU_FLIP_IN_ORDER");
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes);
    auto* restored = reinterpret_cast<std::uint32_t*>(bytes + 8192);
    *restored = 0;
    const std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
    std::copy(nop.begin(), nop.end(), commands);
    const std::uint32_t stored = 7;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(restored), std::as_bytes(std::span(&stored, 1)));
    FrameCall start;
    start.kind = FrameCallKind::FrameStart;
    start.accepted = Base;
    start.completed = Base;
    writer.WriteCall(start);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.busyTimeout = std::chrono::milliseconds(3000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    auto output = std::make_shared<PresentingOutput>(replay.frames, restored);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    FrameCall compute;
    compute.kind = FrameCallKind::Submit;
    compute.ordinal = 1;
    compute.thread = 1;
    compute.queue = 0x20;
    compute.address = address(commands);
    compute.size = nop.size();
    compute.accepted = Base;
    compute.completed = Base;
    compute.values = {Base + 1};
    FrameCall flip;
    flip.kind = FrameCallKind::Flip;
    flip.ordinal = 2;
    flip.thread = 1;
    flip.cpuFlip = true;
    flip.accepted = Base + 1;
    flip.completed = Base;
    flip.values = {7, static_cast<std::uint64_t>(-1), 1, 0, 0};
    auto delta = memoryCall(1, 1);
    delta.ordinal = 3;
    delta.flags = AgcDriver::InFlightDeltaFlag;
    delta.accepted = Base + 1;
    delta.completed = Base;
    delta.values = {1, Base + 1};
    const std::array calls{compute, flip, delta};
    std::string refusal;
    try {
        replay.runWindow(calls);
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    check(refusal.empty(), ("the replay refused a CPU flip that took a serial in the replay and none in the capture, although the delta read in flight behind it lists only the compute submission before the flip: " + refusal).c_str());
    check(*restored == stored, "the replay did not restore the delta read in flight behind a CPU flip that took a serial only in the replay");
    check(replay.frames.Times().size() == 1 && output->seen.load() == 0, ("the replay restored the delta read in flight behind a CPU flip that took a serial only in the replay before that flip presented, while the capture's flip presented before its next call (restored word at the present: " + std::to_string(output->seen.load()) + ")").c_str());
}

void testFlipSerialNeutral(const std::filesystem::path& directory) {
    constexpr std::uint64_t Base = 40;
    if (APS5_ENABLE_TIMING_LOG) return;
    const char* dump = std::getenv("APS5_DUMP_SUBMITS");
    check(std::getenv("APS5_CPU_FLIP_IN_ORDER") == nullptr && std::getenv("APS5_SYNC_FLIP") == nullptr && (dump == nullptr || *dump == '\0'), "the environment orders CPU flips or captures submissions, so the replay's CPU flip would take a serial");
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* commands = reinterpret_cast<std::uint32_t*>(bytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes + 4096);
    auto* restored = reinterpret_cast<std::uint32_t*>(bytes + 8192);
    auto* waiting = reinterpret_cast<std::uint32_t*>(bytes + 12288);
    *label = 0;
    *restored = 0;
    const std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
    std::copy(nop.begin(), nop.end(), commands);
    const std::array<std::uint32_t, 7> wait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(label)), static_cast<std::uint32_t>(address(label) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(wait.begin(), wait.end(), waiting);
    const std::uint32_t stored = 7;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(restored), std::as_bytes(std::span(&stored, 1)));
    FrameCall start;
    start.kind = FrameCallKind::FrameStart;
    start.accepted = Base;
    start.completed = Base;
    writer.WriteCall(start);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    options.paceTimeout = std::chrono::milliseconds(300);
    options.busyTimeout = std::chrono::milliseconds(5000);
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    const auto submit = [&](std::uint64_t ordinal, std::uint32_t queue, const std::uint32_t* words, std::size_t size, std::uint64_t taken) {
        FrameCall call;
        call.kind = FrameCallKind::Submit;
        call.ordinal = ordinal;
        call.thread = 1;
        call.queue = queue;
        call.address = address(words);
        call.size = size;
        call.accepted = taken - 1;
        call.completed = taken - 1;
        call.values = {taken};
        return call;
    };
    FrameCall flip;
    flip.kind = FrameCallKind::Flip;
    flip.ordinal = 2;
    flip.thread = 1;
    flip.cpuFlip = true;
    flip.accepted = Base + 1;
    flip.completed = Base + 1;
    flip.values = {7, static_cast<std::uint64_t>(-1), 1, 0, Base + 2};
    const auto delta = [&](std::vector<std::uint64_t> listed) {
        auto call = memoryCall(1, 1);
        call.ordinal = 4;
        call.flags = AgcDriver::InFlightDeltaFlag;
        call.accepted = Base + 3;
        call.completed = Base;
        call.values = {1};
        call.values.insert(call.values.end(), listed.begin(), listed.end());
        return call;
    };
    const auto refusal = [&](std::span<const FrameCall> calls) {
        std::string error;
        try {
            replay.runWindow(calls);
        } catch (const std::runtime_error& failure) {
            error = failure.what();
        }
        return error;
    };
    const std::array presented{submit(1, 0x20, commands, nop.size(), Base + 1), flip, submit(3, 0, commands, nop.size(), Base + 3), delta({Base + 1, Base + 2, Base + 3})};
    const auto replayed = refusal(presented);
    AgcDriverWaitIdle_nid_postfix();
    check(replayed.empty(), ("the replay refused a CPU flip that took a serial in the capture and none in the replay, so the serials after it could not be mapped: " + replayed).c_str());
    check(*restored == stored && replay.frames.Times().size() == 1, "the replay did not present the CPU flip that took no serial and restore the delta read in flight behind it");
    *restored = 0;
    const auto accepted = progress().first;
    const std::array parked{submit(1, 0x20, commands, nop.size(), Base + 1), flip, submit(3, 0, waiting, wait.size(), Base + 3), delta({Base + 2, Base + 3})};
    const auto gated = refusal(parked);
    std::atomic_ref<std::uint32_t>(*label).store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    const auto expected = "frame replay: memory delta at call 4 waits for submission " + std::to_string(Base + 3) + " (replay serial " + std::to_string(accepted + 2) + "), which had started when the capture read the delta, but no queue runs";
    check(gated.starts_with(expected), ("the delta read in flight behind a CPU flip that took a serial only in the capture did not skip the flip's serial and wait for the submission after it at the replay serial that submission took (" + expected + "): " + gated).c_str());
    check(*restored == 0, "the replay restored a delta read in flight although the submission it waits for never completed");
    auto* computeLabel = reinterpret_cast<std::uint32_t*>(bytes + 16384);
    auto* computeWaiting = reinterpret_cast<std::uint32_t*>(bytes + 20480);
    *computeLabel = 0;
    const std::array<std::uint32_t, 7> computeWait{0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address(computeLabel)), static_cast<std::uint32_t>(address(computeLabel) >> 32u), 1u, 0xffffffffu, 4u};
    std::copy(computeWait.begin(), computeWait.end(), computeWaiting);
    auto flipBehind = flip;
    flipBehind.completed = Base;
    auto graphics = submit(3, 0, commands, nop.size(), Base + 3);
    graphics.completed = Base;
    const auto before = progress().first;
    const std::array earlier{submit(1, 0x20, computeWaiting, computeWait.size(), Base + 1), flipBehind, graphics, delta({Base + 1})};
    const auto gatedEarlier = refusal(earlier);
    std::atomic_ref<std::uint32_t>(*computeLabel).store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    const auto expectedEarlier = "frame replay: memory delta at call 4 waits for submission " + std::to_string(Base + 1) + " (replay serial " + std::to_string(before + 1) + "), which had started when the capture read the delta, but no queue runs";
    check(gatedEarlier.starts_with(expectedEarlier), ("the delta read in flight behind a CPU flip that took a serial only in the capture did not wait for the compute submission before the flip at the replay serial it took (" + expectedEarlier + "): " + gatedEarlier).c_str());
    check(*restored == 0, "the replay restored a delta read in flight while the compute submission before the CPU flip, which it waits for, was parked");
}

void testUnverifiedDeltas(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    const std::uint32_t stored = 7;
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    const auto arena = "{\"arena_base\":" + std::to_string(base);
    const auto refusal = [&](const std::string& name, bool inFlight, const std::optional<std::string>& header, std::optional<std::uint32_t> version = std::nullopt) {
        const auto capture = directory / name;
        {
            FrameCaptureWriter writer(capture);
            writer.WriteRange(address(bytes), FrameCaptureWriter::BlockBytes, FrameRangeKind::Unwatched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
            writer.WriteDelta(1, 1, address(bytes), std::as_bytes(std::span(&stored, 1)));
            auto call = memoryCall(1, 1);
            call.ordinal = 1;
            call.accepted = 2;
            call.completed = inFlight ? 1 : 2;
            if (inFlight) {
                call.flags = AgcDriver::InFlightDeltaFlag;
                call.values.push_back(2);
            }
            writer.WriteCall(call);
            if (header) writer.Finish(*header);
        }
        if (version) rewriteCallsVersion(capture, *version);
        const FrameCaptureReader reader(capture);
        try {
            restoreMemory(reader);
        } catch (const std::exception& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    const auto refused = [&](const std::string& name, const std::optional<std::string>& header, const char* reason) {
        const auto message = refusal(name, true, header);
        check(!message.empty(), ("the replay accepted capture " + name + ", whose memory delta was read with submissions in flight and never verified").c_str());
        check(message.find(reason) != std::string::npos, ("the replay refused capture " + name + " without '" + reason + "': " + message).c_str());
    };
    refused("missing", std::nullopt, "capture.json is missing");
    refused("bare", arena + "}", "capture.json has no stale_delta");
    refused("unrecorded", arena + ",\"stale_delta\":0,\"warnings\":[]}", "capture.json has no unverified_deltas");
    refused("unparsed", arena + ",\"stale_delta\":0,\"unverified_deltas\":null,\"warnings\":[]}", "unverified_deltas is not a number");
    refused("unverified", arena + ",\"stale_delta\":0,\"unverified_deltas\":1,\"warnings\":[]}", "capture.json has unverified_deltas 1");
    refused("stale", arena + ",\"stale_delta\":1,\"unverified_deltas\":0,\"warnings\":[\"stopped: memory delta 1 was read while submission 2, in flight at its read, stored into its page 0x10000\"]}", "(stale_delta 1)");
    const auto verified = refusal("verified", true, arena + ",\"stale_delta\":0,\"unverified_deltas\":0,\"warnings\":[\"a \\\"stopped: quoted\\\" warning \\\\ \\u0009\",\"4096 bytes under multisampled targets whose samples are only on the host are zero in the capture, so replay starts those targets without their earlier contents\"]}");
    check(verified.empty(), ("the replay refused a capture whose memory delta read in flight was verified: " + verified).c_str());
    const auto stopped = refusal("stopped", true, arena + ",\"stale_delta\":0,\"unverified_deltas\":0,\"warnings\":[\"stopped: less than 4 GiB free on the capture volume\"]}");
    check(stopped.empty(), ("the replay refused a capture that stopped early, although the check after its stop verified its memory delta read in flight: " + stopped).c_str());
    const auto held = refusal("held", false, arena + ",\"unverified_deltas\":1,\"warnings\":[\"stopped: less than 4 GiB free on the capture volume\"]}");
    check(held.empty(), ("the replay refused a capture without memory deltas read in flight: " + held).c_str());
    const auto unheaded = refusal("unheaded", false, std::nullopt);
    check(unheaded.find("capture.json is missing") != std::string::npos, ("the replay did not refuse capture unheaded, which has no memory delta read in flight but no capture.json either, so its last records may name memory that was never written: " + unheaded).c_str());
    const auto unheadedPrevious = refusal("unheaded_previous", false, std::nullopt, AgcDriver::InFlightCallsVersion - 1);
    check(unheadedPrevious.empty(), ("the replay refused capture unheaded_previous, a capture of the previous calls format without capture.json, which that format never needed: " + unheadedPrevious).c_str());
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

void testDeltaPaths(const std::filesystem::path& directory, std::string_view mode) {
    constexpr std::size_t Block = FrameCaptureWriter::BlockBytes;
    constexpr std::size_t PieceBytes = Block + Block / 2;
    const bool host = mode != "shader_unaligned_restored";
    const bool rewritten = mode == "shader_host_rewritten";
    const std::size_t codeBytes = host ? 3 * Block : Block;
    Shader* shader = nullptr;
    std::byte* code = nullptr;
    if (host) {
        shader = reinterpret_cast<Shader*>(allocate(1u << 20));
        std::byte* memory = nullptr;
#ifdef _WIN32
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, codeBytes + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#elif defined(__linux__)
        void* mapped = mmap(nullptr, codeBytes + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memory = mapped != MAP_FAILED ? static_cast<std::byte*>(mapped) : nullptr;
#endif
        check(memory != nullptr, "host shader allocation failed");
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, codeBytes + 4096, true, true, true);
        code = memory + 256;
    } else {
        auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * Block, Block));
        check(block != nullptr, "guest reservation failed");
        GuestArena::GuestArenaRelease_nid_postfix(block, 2 * Block);
        auto* piece = block + Block / 2;
        GuestArena::GuestArenaCommit_nid_postfix(piece, PieceBytes, 0x04, PieceBytes);
#ifdef _WIN32
        AgcDriver::GuestMemory::Unwatch(address(piece), PieceBytes);
#endif
        check(!AgcDriver::GuestMemory::Watched(address(piece), PieceBytes), "the unaligned shader range is write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(piece, PieceBytes, true, true, false);
        shader = reinterpret_cast<Shader*>(piece);
        code = piece + 256;
    }
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    shader->shader_size = codeBytes;
    shader->code = code;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    const auto store = [&](unsigned seed) {
        std::vector<std::byte> bytes(codeBytes);
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(i * 31 + seed);
        std::memcpy(code, bytes.data(), bytes.size());
        return bytes;
    };
    const auto checked = rewritten ? FrameCallKind::RegisterShader : FrameCallKind::Flip;
    const auto call = [&](FrameCallKind kind) {
        if (kind == FrameCallKind::Flip) recorder->Flip(AgcDriver::FlipInfo{}, {});
        else recorder->RegisterShader(shader);
    };
    const auto expected = store(17);
    call(checked);
    store(29);
    call(rewritten ? FrameCallKind::Flip : FrameCallKind::RegisterShader);
    store(17);
    call(checked);
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    CapturedMemory memory(reader);
    std::map<std::uint64_t, std::vector<const FrameMemoryChunk*>> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) groups[chunk.group].push_back(&chunk);
    }
    std::size_t seen = 0;
    std::optional<std::vector<std::byte>> replayed;
    for (const auto& captured : reader.Calls()) {
        if (captured.kind == checked && ++seen == 2) {
            replayed = memory.Read(address(code), codeBytes);
            break;
        }
        if (captured.kind != FrameCallKind::Memory) continue;
        if (const auto found = groups.find(captured.values.at(0)); found != groups.end()) memory.Apply(found->second);
    }
    check(seen == 2, "the capture lost a call of the test");
    check(replayed && *replayed == expected, rewritten ? "the second registration replays the host code a flip's delta wrote, not the code the title registered again" :
        host ? "after the second flip the replay keeps the host code a shader delta wrote, not the code the title restored" :
        "after the second flip the replay keeps the code a shader delta wrote into an unaligned unwatched range, not the code the title restored");
}

bool insideRanges(const FrameCaptureReader& reader, const FrameMemoryChunk& chunk) {
    std::map<std::uint64_t, std::uint64_t> ranges;
    for (const auto& range : reader.Chunks()) {
        if (range.kind == FrameMemoryKind::Range) ranges[range.address] = std::max(ranges[range.address], range.address + range.bytes);
    }
    auto cursor = chunk.address;
    for (const auto& [begin, end] : ranges) {
        if (begin <= cursor && cursor < end) cursor = end;
    }
    return cursor >= chunk.address + chunk.bytes;
}

void testShaderBlockHole(const std::filesystem::path& directory) {
    constexpr std::size_t Block = FrameCaptureWriter::BlockBytes;
    constexpr std::size_t Page = 4096;
    auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * Block, Block));
    check(block != nullptr, "guest reservation failed");
    GuestArena::GuestArenaRelease_nid_postfix(block, 2 * Block);
    auto* tail = block + 8 * Page;
    for (const auto& [piece, bytes] : {std::pair{block, 7 * Page}, std::pair{tail, Block - 8 * Page}}) {
        GuestArena::GuestArenaCommit_nid_postfix(piece, bytes, 0x04, bytes);
#ifdef _WIN32
        AgcDriver::GuestMemory::Unwatch(address(piece), bytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(piece, bytes, true, true, false);
    }
    auto* shader = reinterpret_cast<Shader*>(tail + 0x2b0);
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
    bool shaderDelta = false;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta) continue;
        check(insideRanges(reader, chunk), "a memory delta of the shader's block reaches a page that no range of the capture holds");
        shaderDelta = shaderDelta || (chunk.address <= address(shader) && address(shader) + sizeof(Shader) <= chunk.address + chunk.bytes);
    }
    check(shaderDelta, "the capture holds no memory delta with the registered shader");
}

void testDeltaOutsideRanges(const std::filesystem::path& directory) {
    constexpr std::size_t Block = FrameCaptureWriter::BlockBytes;
    constexpr std::size_t Page = 4096;
    auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Block, Block));
    check(block != nullptr, "guest reservation failed");
    GuestArena::GuestArenaRelease_nid_postfix(block, Block);
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    const std::vector<std::byte> delta(Block, std::byte{0x5a});
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(block), 7 * Page, FrameRangeKind::Section, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteRange(address(block) + 8 * Page, Block - 8 * Page, FrameRangeKind::Section, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteDelta(2, 0, address(block), delta);
    writer.WriteCall(memoryCall(2, 1));
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    std::string message;
    try {
        restoreMemory(reader);
    } catch (const std::exception& error) {
        message = error.what();
    }
    char hole[48];
    std::snprintf(hole, sizeof(hole), "reaches 0x%llx", static_cast<unsigned long long>(address(block) + 7 * Page));
    check(message.find(hole) != std::string::npos, ("the replay accepted a memory delta across a page that no range of the capture maps: " + message).c_str());
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
        else if (mode.starts_with("shader_prologue_")) testShaderCode(directory, true, mode);
        else if (mode == "shared_page") testSharedPage(directory);
        else if (mode == "host_neighbors") testHostNeighbors(directory);
        else if (mode == "unwatched") testUnwatched();
        else if (mode == "tokens") testEopTokens();
        else if (mode == "reservations" || mode == "reservation_failure") testRepeatedReservations(directory, mode == "reservation_failure");
        else if (mode == "blocking") testBlockingSubmission(directory);
        else if (mode == "gate_settled") testGateSettled(directory);
        else if (mode == "depth_submits") testDepthAfterSubmits(directory);
        else if (mode == "depth_passes") testDepthOfTimedPasses(directory);
        else if (mode == "gate_failure") testGateFailure(directory);
        else if (mode == "in_flight_breaker" || mode == "previous_breaker") testRewindBreaker(directory, mode == "previous_breaker");
        else if (mode == "unverified") testUnverifiedDeltas(directory);
        else if (mode == "serial_diverged") testSerialDiverged(directory);
        else if (mode == "serial_flip_in_order") testSerialFlipInOrder(directory);
        else if (mode == "flip_serial_neutral") testFlipSerialNeutral(directory);
        else if (mode == "environment") testColdPipelines();
        else if (mode == "shader" || mode == "shader_host") testShaderCode(directory, mode == "shader_host");
        else if (mode == "shader_host_rewritten" || mode == "shader_host_restored" || mode == "shader_unaligned_restored") testDeltaPaths(directory, mode);
        else if (mode == "shader_hole") testShaderBlockHole(directory);
        else if (mode == "delta_hole") testDeltaOutsideRanges(directory);
        else throw std::runtime_error("unknown test");
        if (mode != "reservation_failure" && mode != "gate_failure") AgcDriverShutdown_nid_postfix();
        std::puts("frame replay state test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
