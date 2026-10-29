#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace AgcDriver::DriverDetail {
struct DriverTestAccess {
    static void OpenDevice() {
        auto& driver = Driver::Get();
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (driver.device == nullptr) driver.device = std::make_shared<VulkanDevice>();
    }
};
}

namespace {

using AgcDriver::FrameCallKind;
using AgcDriver::FrameCaptureReader;
using AgcDriver::FrameMemoryKind;

constexpr std::uint32_t Handle = 7;
constexpr std::uint8_t GraphicsMemory = 0x0a;
constexpr std::uint8_t ComputeMemory = 5;
constexpr std::uint8_t DmaMemory = 0;
constexpr std::size_t BlockBytes = 1u << 20;
constexpr std::size_t CommandBytes = 64u * 1024u;
constexpr std::size_t DataBytes = 4096;
constexpr std::uint32_t Done = 1;

struct Inconclusive : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t at(const volatile void* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

class Request final : public AgcDriver::IFlipRequest {
public:
    explicit Request(std::atomic<int>& ready) : ready(ready) {}
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override { ++ready; }
    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
    }

private:
    std::atomic<int>& ready;
};

class Output final : public AgcDriver::IVideoOutput {
public:
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<Request>(ready); }
    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
    }
    std::atomic<int> ready = 0;
};

struct Layout {
    std::byte* base = nullptr;
    std::uint32_t* firstCommands = nullptr;
    std::uint32_t* secondCommands = nullptr;
    std::byte* source = nullptr;
    std::byte* firstTarget = nullptr;
    std::byte* secondTarget = nullptr;
    std::uint32_t* label = nullptr;
};

Layout allocate(std::size_t bytes = BlockBytes) {
    auto* const block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    check(block != nullptr, "cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, 0x04, bytes);
#ifdef __linux__
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true, true);
    }
    Layout layout;
    layout.base = block;
    layout.firstCommands = reinterpret_cast<std::uint32_t*>(block);
    layout.secondCommands = reinterpret_cast<std::uint32_t*>(block + CommandBytes);
    layout.source = block + 2 * CommandBytes;
    layout.firstTarget = layout.source + DataBytes;
    layout.secondTarget = layout.firstTarget + DataBytes;
    layout.label = reinterpret_cast<std::uint32_t*>(layout.secondTarget + DataBytes);
    return layout;
}

std::vector<std::byte> pattern(std::uint8_t seed) {
    std::vector<std::byte> bytes(DataBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<std::byte>((index * 31 + seed) & 0xff);
    return bytes;
}

std::uint32_t encodeCopy(std::uint32_t* words, std::size_t commandBytes, const std::byte* source, std::byte* target, std::uint32_t* label) {
    CommandBuffer buffer{words, words + commandBytes / sizeof(std::uint32_t), words, words + commandBytes / sizeof(std::uint32_t), nullptr, nullptr, 0};
    sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(target), DmaMemory, 0, at(source), DataBytes, 0, 1, 0);
    sceAgcDcbWriteData(&buffer, GraphicsMemory, 0, at(label), &Done, 1, 0, 1);
    return static_cast<std::uint32_t>(buffer.cursor_up - words);
}

void copy(std::uint32_t* words, std::size_t commandBytes, const std::byte* source, std::byte* target, std::uint32_t* label) {
    Packet packet{words, encodeCopy(words, commandBytes, source, target, label), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

void copyFrame(std::uint32_t* words, const Layout& layout, std::byte* target) {
    copy(words, CommandBytes, layout.source, target, layout.label);
}

void flip() {
    AgcDriverSubmitFlip_nid_postfix(AgcDriver::FlipInfo{Handle, -1, 1, 0});
}

std::vector<std::byte> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    check(file.is_open(), "cannot open " + path.string());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<std::byte> result(bytes.size());
    std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

bool equal(const std::byte* bytes, const std::vector<std::byte>& expected) {
    return std::memcmp(bytes, expected.data(), expected.size()) == 0;
}

void testCaptureAndReplay(const std::filesystem::path& tool, bool pipelines) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const auto layout = allocate();
    const auto first = pattern(3);
    const auto second = pattern(101);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    AgcDriverUnregisterVideoOutput_nid_postfix(Handle, output);
    output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    std::memcpy(layout.source, first.data(), first.size());
    copyFrame(layout.firstCommands, layout, layout.firstTarget);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverUnregisterVideoOutput_nid_postfix(Handle, output);
    output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    const auto late = allocate();
    std::memcpy(late.source, second.data(), second.size());
    copyFrame(late.firstCommands, late, late.firstTarget);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, first) && equal(late.firstTarget, second), "the driver did not run the copies while capturing");

    const FrameCaptureReader reader(capture);
    check(!reader.Truncated(), "the capture is truncated");
    check(reader.Header().find("\"frames\":2") != std::string::npos, "the capture header does not record two frames: " + reader.Header());
    std::size_t submits = 0, flips = 0, memory = 0, starts = 0, registrations = 0, unregistrations = 0;
    for (const auto& call : reader.Calls()) {
        submits += call.kind == FrameCallKind::Submit;
        flips += call.kind == FrameCallKind::Flip;
        memory += call.kind == FrameCallKind::Memory;
        starts += call.kind == FrameCallKind::FrameStart;
        registrations += call.kind == FrameCallKind::RegisterVideoOutput;
        unregistrations += call.kind == FrameCallKind::UnregisterVideoOutput;
    }
    check(starts == 1 && submits == 2 && flips == 2, "the capture holds " + std::to_string(submits) + " submits and " + std::to_string(flips) + " flips after " + std::to_string(starts) + " frame starts");
    check(memory >= 2, "the CPU writes before each copy were not recorded as memory deltas");
    check(registrations == 3 && unregistrations == 2, "video output close and reopen was not recorded before and during capture");
    const auto recorded = [&](const std::byte* base) {
        return std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) {
            return chunk.kind == FrameMemoryKind::Range && chunk.address <= at(base) && at(base) + BlockBytes <= chunk.address + chunk.bytes;
        });
    };
    check(recorded(layout.base), "the guest block is not in the snapshot ranges");
    check(recorded(late.base), "the block committed during the capture is not in its ranges");
    check(reader.Header().find("\"late_ranges\":1") != std::string::npos, "the capture header does not count one range committed during the capture: " + reader.Header());

    const auto firstDump = capture / "first dump.bin";
    const auto secondDump = capture / "second dump.bin";
    const auto replay = [&](const std::string& options, const std::filesystem::path& output) {
        std::filesystem::remove(firstDump);
        std::filesystem::remove(secondDump);
        auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict " + options + " --dump " + std::to_string(at(layout.firstTarget)) + ":" + std::to_string(DataBytes) + ":\"" +
                       firstDump.string() + "\" --dump " + std::to_string(at(late.firstTarget)) + ":" + std::to_string(DataBytes) + ":\"" + secondDump.string() + "\" > \"" + output.string() + "\" 2> \"" + output.string() + ".log\"";
#ifdef _WIN32
        command = "\"" + command + "\"";
#endif
        const auto code = std::system(command.c_str());
        check(code == 0, "agc_frame_replay " + options + " exited with " + std::to_string(code));
        check(readFile(firstDump) == first, "the replay " + options + " did not reproduce the first copy");
        check(readFile(secondDump) == second, "the replay " + options + " did not reproduce the second copy");
        const auto json = readFile(output);
        return std::string(reinterpret_cast<const char*>(json.data()), json.size());
    };
    replay("", capture / "replay output.json");
    if (pipelines) {
        check(std::getenv("APS5_HEADLESS_PIPELINE_CACHE") != nullptr, "the pipeline cache switch was not inherited");
        const auto warm = capture / "warm output.json";
        replay("", warm);
        const auto warmLog = readFile(warm.string() + ".log");
        check(std::string(reinterpret_cast<const char*>(warmLog.data()), warmLog.size()).find("[pipeline-cache] loaded ") != std::string::npos, "the warm replay did not load the seeded pipeline cache");
        const auto cold = capture / "cold output.json";
        replay("--cold-pipelines", cold);
        const auto coldLog = readFile(cold.string() + ".log");
        check(std::string(reinterpret_cast<const char*>(coldLog.data()), coldLog.size()).find("[pipeline-cache] loaded ") == std::string::npos, "--cold-pipelines loaded a cache with APS5_HEADLESS_PIPELINE_CACHE inherited");
        return;
    }
    const auto repeated = replay("--repeat 3", capture / "repeat output.json");
    std::size_t passes = 0;
    for (auto found = repeated.find("\"frames\":2,\"p50\""); found != std::string::npos; found = repeated.find("\"frames\":2,\"p50\"", found + 1)) ++passes;
    check(passes == 3, "the repeated replay did not replay both frames in each of three passes: " + repeated);
}

void testConcurrentCopies(const std::filesystem::path& tool) {
    constexpr std::size_t Copies = 64;
    constexpr std::size_t Frames = 8;
    constexpr std::size_t CopyCommandBytes = 1024;
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const auto layout = allocate();
    const auto other = allocate();
    auto* const otherSource = other.base + 128 * 1024;
    auto* const targets = otherSource + DataBytes;
    auto* const otherLabel = reinterpret_cast<std::uint32_t*>(other.base + 512 * 1024);
    const auto copied = pattern(57);
    std::memcpy(otherSource, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    std::thread copier([&] {
        for (std::size_t index = 0; index < Copies; ++index) {
            copy(reinterpret_cast<std::uint32_t*>(other.base + index * CopyCommandBytes), CopyCommandBytes, otherSource, targets + index * DataBytes, otherLabel);
        }
    });
    for (std::size_t frame = 0; frame + 1 < Frames; ++frame) {
        const auto bytes = pattern(static_cast<std::uint8_t>(frame));
        std::memcpy(layout.source, bytes.data(), bytes.size());
        copyFrame(layout.firstCommands, layout, layout.firstTarget);
        flip();
    }
    copier.join();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const FrameCaptureReader reader(capture);
    check(!reader.Truncated(), "the capture is truncated");
    std::uint64_t base = 0;
    std::size_t submits = 0, memory = 0;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::FrameStart) base = call.completed;
        if (call.kind == FrameCallKind::Memory) {
            ++memory;
            check(call.completed >= base + submits, "a memory delta was read with " + std::to_string(base + submits - call.completed) + " of the " + std::to_string(submits) + " submissions recorded before it unfinished");
        }
        submits += call.kind == FrameCallKind::Submit;
    }
    check(submits == Copies + Frames - 1, "the capture holds " + std::to_string(submits) + " submits, not " + std::to_string(Copies + Frames - 1));
    check(memory != 0, "the capture holds no memory deltas");

    const auto dump = capture / "targets.bin";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --dump " + std::to_string(at(targets)) + ":" + std::to_string(Copies * DataBytes) + ":\"" + dump.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    check(code == 0, "agc_frame_replay exited with " + std::to_string(code));
    const auto replayed = readFile(dump);
    check(replayed.size() == Copies * DataBytes, "the replay dumped " + std::to_string(replayed.size()) + " bytes of the copy targets");
    for (std::size_t index = 0; index < Copies; ++index) {
        check(std::memcmp(replayed.data() + index * DataBytes, copied.data(), DataBytes) == 0, "the replay did not reproduce copy " + std::to_string(index) + " of the second thread");
    }
}

void testReservationError(const std::filesystem::path& tool) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    AgcDriver::FrameCaptureWriter writer(capture);
    AgcDriver::FrameCall output;
    output.kind = FrameCallKind::RegisterVideoOutput;
    output.values = {Handle};
    writer.WriteCall(output);
    AgcDriver::FrameCall reservation;
    reservation.kind = FrameCallKind::ReserveEopFlip;
    reservation.values = {Handle, static_cast<std::uint64_t>(-1), 1, 0, AgcDriver::EopFlipTokenBase};
    writer.WriteCall(reservation);
    AgcDriver::FrameCall start;
    start.kind = FrameCallKind::FrameStart;
    writer.WriteCall(start);
    output.kind = FrameCallKind::UnregisterVideoOutput;
    writer.WriteCall(output);
    output.kind = FrameCallKind::RegisterVideoOutput;
    writer.WriteCall(output);
    reservation.values[3] = 1;
    writer.WriteCall(reservation);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const auto result = capture / "reservation error.json";
    const auto log = capture / "reservation error.log";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --repeat 2 --strict > \"" + result.string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    check(std::system(command.c_str()) != 0, "the replay accepted an incompatible reservation on the second pass");
    const auto errors = readFile(log);
    check(std::string(reinterpret_cast<const char*>(errors.data()), errors.size()).find("EOP flip token is reserved for a different flip") != std::string::npos, "the repeated replay did not report the incompatible reservation");
    const auto report = readFile(result);
    check(std::string(reinterpret_cast<const char*>(report.data()), report.size()).find("\"p50\"") == std::string::npos, "a failed repeated reservation produced a successful replay report");
}

void testParkedRewindReplays(const std::filesystem::path& tool) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const auto layout = allocate();
    const auto copied = pattern(77);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const words = layout.firstCommands;
    const std::array<std::uint32_t, 6> commands{0xc0005900u, 0u, 0xc0023f00u, 0u, 0u, 1u << 20u};
    std::memcpy(words, commands.data(), sizeof(commands));
    Packet packet{words, static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the REWIND submission failed");
    const auto parked = [&] {
        std::array<AgcDriver::AwaitedWait, 64> awaited{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(awaited.data(), awaited.size());
        return std::any_of(awaited.begin(), awaited.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(words + 1); });
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!parked() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(parked(), "the queue did not park at the unreleased REWIND");
    const auto segment = encodeCopy(layout.secondCommands, CommandBytes, layout.source, layout.firstTarget, layout.label);
    words[3] = static_cast<std::uint32_t>(at(layout.secondCommands));
    words[4] = static_cast<std::uint32_t>(at(layout.secondCommands) >> 32u);
    words[5] = (1u << 20u) | segment;
    std::atomic_ref<std::uint32_t>(words[1]).store(AgcDriver::Pm4::RewindValid, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not run the copy chained from the released REWIND tail");
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const FrameCaptureReader reader(capture);
    check(!reader.Truncated(), "the capture is truncated");
    const auto submits = std::count_if(reader.Calls().begin(), reader.Calls().end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(words); });
    check(submits == 1, "the capture does not hold the REWIND submission once");

    const auto dump = capture / "rewind.bin";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --dump " + std::to_string(at(layout.firstTarget)) + ":" + std::to_string(DataBytes) + ":\"" + dump.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    check(code == 0, "agc_frame_replay exited with " + std::to_string(code));
    check(readFile(dump) == copied, "the replay did not run the copy chained from the released REWIND tail");
}

CommandBuffer commands(std::uint32_t* words) {
    return CommandBuffer{words, words + CommandBytes / sizeof(std::uint32_t), words, words + CommandBytes / sizeof(std::uint32_t), nullptr, nullptr, 0};
}

std::uint32_t* submitRewound(std::uint32_t* words, const std::function<void(CommandBuffer&)>& before, const std::function<void(CommandBuffer&)>& tail) {
    auto buffer = commands(words);
    before(buffer);
    auto* const control = sceAgcDcbRewind(&buffer, 0) + 1;
    tail(buffer);
    Packet packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    return control;
}

void release(std::uint32_t* word) {
    std::atomic_ref<std::uint32_t>(*word).store(AgcDriver::Pm4::RewindValid, std::memory_order_release);
}

void writeDone(CommandBuffer& buffer, const std::uint32_t* label) {
    sceAgcDcbWriteData(&buffer, GraphicsMemory, 0, at(label), &Done, 1, 0, 1);
}

void releaseMem(CommandBuffer& buffer, const std::uint32_t* label) {
    const std::array<std::uint32_t, 8> words{0xc0064900u, 0x06200504u, 0x20000000u, static_cast<std::uint32_t>(at(label)), static_cast<std::uint32_t>(at(label) >> 32u), Done, 0u, 0u};
    std::memcpy(buffer.cursor_up, words.data(), sizeof(words));
    buffer.cursor_up += words.size();
}

void waitUntil(const std::function<bool()>& done, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!done()) {
        check(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::filesystem::path checkCapture(std::uint64_t inFlight) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const FrameCaptureReader reader(capture);
    check(!reader.Truncated(), "the capture is truncated");
    check(reader.Header().find("stopped") == std::string::npos, "the capture stopped: " + reader.Header());
    check(reader.Header().find("\"frames\":2") != std::string::npos, "the capture header does not record two frames: " + reader.Header());
    const bool parked = std::any_of(reader.Calls().begin(), reader.Calls().end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.accepted - call.completed == inFlight; });
    check(parked, "no memory delta was read while " + std::to_string(inFlight) + " submissions waited for the CPU");
    return capture;
}

std::vector<std::byte> replayed(const std::filesystem::path& tool, const std::filesystem::path& capture, const void* address, std::size_t bytes, const std::string& options = "") {
    const auto dump = capture / "dump.bin";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict" + options + " --dump " + std::to_string(at(address)) + ":" + std::to_string(bytes) + ":\"" + dump.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    check(code == 0, "agc_frame_replay exited with " + std::to_string(code));
    return readFile(dump);
}

void testStoreAfterGatedCopy(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto copied = pattern(7);
    const auto stored = pattern(211);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const gate = layout.label + 1;
    auto* const control = submitRewound(layout.firstCommands, [&](CommandBuffer& buffer) {
        sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
        sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    }, [&](CommandBuffer& buffer) { writeDone(buffer, layout.label); });
    std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    waitUntil([&] { return equal(layout.firstTarget, copied); }, "the copy before the REWIND did not land");
    std::memcpy(layout.firstTarget, stored.data(), stored.size());
    flip();
    release(control);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(*layout.label == Done && equal(layout.firstTarget, stored), "the driver did not run the tail after the CPU store");
    check(replayed(tool, checkCapture(1), layout.firstTarget, DataBytes) == stored, "the replay applied the CPU store before the copy that preceded it");
}

void testCopyInFlightAtDelta(const std::filesystem::path& tool) {
    constexpr std::size_t CopyBytes = 32u << 20;
    constexpr std::size_t Stored = 64;
    const auto layout = allocate();
    const auto big = allocate(2 * CopyBytes);
    auto* const source = big.base;
    auto* const target = big.base + CopyBytes;
    for (std::size_t index = 0; index < CopyBytes; ++index) source[index] = static_cast<std::byte>((index * 13 + 5) & 0xff);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const control = submitRewound(layout.firstCommands, [&](CommandBuffer& buffer) {
        sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(target + Stored), DmaMemory, 0, at(source), CopyBytes - Stored, 0, 1, 0);
    }, [&](CommandBuffer& buffer) { writeDone(buffer, layout.label); });
    std::memset(target, 0x5a, Stored);
    const auto gpuWaits = AgcDriver::Graphics::Recorder::ThreadHookWaits();
    flip();
    const bool inFlight = AgcDriver::Graphics::Recorder::ThreadHookWaits() > gpuWaits;
    release(control);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    std::vector<std::byte> expected(DataBytes, std::byte{0x5a});
    std::memcpy(expected.data() + Stored, source, DataBytes - Stored);
    check(equal(target, expected), "the driver did not run the copy beside the CPU store");
    check(replayed(tool, checkCapture(1), target, DataBytes) == expected, "the memory delta was read before the copy that preceded the REWIND landed");
    if (!inFlight) throw Inconclusive("the GPU copy finished before the memory delta was read");
}

void testComputeWaitBehindRewind(const std::filesystem::path& tool) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    const auto layout = allocate();
    const auto stored = pattern(99);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const released = layout.label + 1;
    auto* const control = submitRewound(layout.firstCommands, [](CommandBuffer&) {}, [&](CommandBuffer& buffer) { writeDone(buffer, released); });
    auto compute = commands(layout.secondCommands);
    sceAgcAcbWaitRegMem(&compute, 0, 3, 0, released, Done, 0xffffffffu, 0);
    sceAgcAcbWriteData(&compute, ComputeMemory, 0, at(layout.label), &Done, 1, 0, 1);
    Packet packet{layout.secondCommands, static_cast<std::uint32_t>(compute.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "compute submission failed");
    std::memcpy(layout.source, stored.data(), stored.size());
    flip();
    release(control);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(*layout.label == Done, "the compute queue did not pass its wait");
    const auto capture = checkCapture(2);
    check(replayed(tool, capture, layout.label, 4) == std::vector<std::byte>{std::byte{Done}, std::byte{0}, std::byte{0}, std::byte{0}}, "the replay did not run the compute queue past its wait");
}

void testLabelFromLaterSubmission(const std::filesystem::path& tool) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    const auto layout = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const awaited = layout.label + 1;
    auto compute = commands(layout.secondCommands);
    sceAgcAcbWaitRegMem(&compute, 0, 3, 0, awaited, Done, 0xffffffffu, 0);
    sceAgcAcbWriteData(&compute, ComputeMemory, 0, at(layout.label), &Done, 1, 0, 1);
    Packet consumer{layout.secondCommands, static_cast<std::uint32_t>(compute.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &consumer) == 0, "compute submission failed");
    waitUntil([&] {
        std::array<AgcDriver::AwaitedWait, 8> waits{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
        return std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(awaited); });
    }, "the compute wait never reached the awaited-wait registry");
    *layout.source = std::byte{0x77};
    auto graphics = commands(layout.firstCommands);
    writeDone(graphics, awaited);
    Packet producer{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    const auto began = std::chrono::steady_clock::now();
    check(sceAgcDriverSubmitDcb(&producer) == 0, "graphics submission failed");
    const auto submitted = std::chrono::steady_clock::now() - began;
    check(submitted < std::chrono::seconds(2), "the producer submission waited " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(submitted).count()) + " ms for the capture");
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the compute queue did not pass its wait");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(1), layout.label, 4) == std::vector<std::byte>{std::byte{Done}, std::byte{0}, std::byte{0}, std::byte{0}}, "the replay did not run the compute queue past the label the later submission wrote");
}

void testReleaseDuringDelta(const std::filesystem::path& tool) {
    constexpr std::size_t BulkBytes = 256u << 20;
    const auto layout = allocate();
    const auto bulk = allocate(BulkBytes);
    auto* const target = bulk.base + BulkBytes - DataBytes;
    const auto copied = pattern(173);
    const auto stored = pattern(31);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const gate = layout.label + 1;
    auto* const control = submitRewound(layout.firstCommands, [&](CommandBuffer& buffer) {
        sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
        sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(target), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    }, [&](CommandBuffer& buffer) { writeDone(buffer, layout.label); });
    for (std::size_t offset = 0; offset + DataBytes < BulkBytes; offset += 2 * DataBytes) std::memset(bulk.base + offset, 0x6b, DataBytes);
    std::memcpy(target, stored.data(), stored.size());
    std::atomic<bool> flipping{false};
    std::thread opener([&] {
        while (!flipping.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    });
    flipping.store(true);
    flip();
    opener.join();
    release(control);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(*layout.label == Done && equal(target, copied), "the driver did not run the copy after the gate opened");
    const auto capture = checkCapture(1);
    const FrameCaptureReader reader(capture);
    const auto parked = std::find_if(reader.Calls().begin(), reader.Calls().end(), [](const auto& call) { return call.kind == FrameCallKind::Memory && call.completed < call.accepted; });
    const auto chunk = std::find_if(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) { return chunk.kind == FrameMemoryKind::Delta && chunk.group == parked->values[0] && chunk.address <= at(target) && at(target) < chunk.address + chunk.bytes; });
    check(chunk != reader.Chunks().end(), "the parked memory delta does not hold the CPU store");
    const auto bytes = reader.Read(*chunk);
    check(std::equal(stored.begin(), stored.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at(target) - chunk->address)), "the memory delta read while the queue waited on its gate holds the copy the queue made after the gate opened");
    check(replayed(tool, capture, target, DataBytes) == copied, "the replay did not run the copy after the gate");
}

void testFilledRewindTail(const std::filesystem::path& tool) {
    const auto layout = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    auto called = commands(layout.secondCommands);
    writeDone(called, layout.label);
    const auto build = [&](const std::uint32_t* target, std::uint32_t words) {
        auto buffer = commands(layout.firstCommands);
        auto* const control = sceAgcDcbRewind(&buffer, 0) + 1;
        sceAgcDcbJump(&buffer, 0, 0, target, words);
        return std::pair{control, Packet{layout.firstCommands, static_cast<std::uint32_t>(buffer.cursor_up - layout.firstCommands), 0, {}}};
    };
    auto [control, packet] = build(nullptr, 0);

    flip();
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    build(layout.secondCommands, static_cast<std::uint32_t>(called.cursor_up - layout.secondCommands));
    release(control);
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the filled REWIND tail did not run");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(0), layout.label, 4) == std::vector<std::byte>{std::byte{Done}, std::byte{0}, std::byte{0}, std::byte{0}}, "the replay did not run the REWIND tail the title filled in the delta that released it");
}

void testRewindFilledAcrossChunks(const std::filesystem::path& tool) {
    constexpr std::size_t BulkBytes = 64u << 20;
    const auto layout = allocate();
    const auto bulk = allocate(BulkBytes);
    const auto jumped = allocate();
    check(at(layout.base) < at(bulk.base) && at(bulk.base) < at(jumped.base), "the guest allocations do not ascend");
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    auto called = commands(layout.secondCommands);
    writeDone(called, layout.label);
    const auto words = static_cast<std::uint32_t>(called.cursor_up - layout.secondCommands);
    auto buffer = commands(layout.firstCommands);
    auto* const control = sceAgcDcbRewind(&buffer, 0) + 1;
    sceAgcDcbJump(&buffer, 0, 0, jumped.firstCommands, words);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(buffer.cursor_up - layout.firstCommands), 0, {}};

    flip();
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    std::memcpy(jumped.firstCommands, layout.secondCommands, words * sizeof(std::uint32_t));
    for (std::size_t offset = 0; offset < BulkBytes; offset += 4096) bulk.base[offset] = std::byte{1};
    release(control);
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the REWIND tail did not run");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(0), layout.label, 4) == std::vector<std::byte>{std::byte{Done}, std::byte{0}, std::byte{0}, std::byte{0}}, "the replay resumed the REWIND before the delta restored the buffer its tail jumps to");
}

void testGpuStoreBehindRewind(const std::filesystem::path& tool) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    constexpr std::uint32_t Filled = 0x5a5a5a5au;
    constexpr std::size_t FillBytes = 0x3fff000u;
    constexpr std::size_t Fills = 4;
    const auto layout = allocate();
    const auto bulk = allocate(Fills * FillBytes);
    const auto read = pattern(41);
    std::memcpy(layout.source, read.data(), read.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const released = layout.label + 1;
    auto* const title = layout.label + 2;
    auto* const filled = reinterpret_cast<const std::uint32_t*>(bulk.base + Fills * FillBytes) - 1;
    auto graphics = commands(layout.firstCommands);
    auto* const first = sceAgcDcbRewind(&graphics, 0) + 1;
    for (std::size_t fill = 0; fill < Fills; ++fill) sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(bulk.base + fill * FillBytes), 2, 0, Filled, FillBytes, 0, 1, 0);
    sceAgcDcbWaitRegMem(&graphics, 0, 3, 0, 0, filled, Filled, 0xffffffffu, 0);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    releaseMem(graphics, released);
    auto* const second = sceAgcDcbRewind(&graphics, 0) + 1;
    writeDone(graphics, layout.label);
    Packet producer{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&producer) == 0, "graphics submission failed");
    auto compute = commands(layout.secondCommands);
    sceAgcAcbWaitRegMem(&compute, 0, 3, 0, released, Done, 0xffffffffu, 0);
    sceAgcAcbDmaData(&compute, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(layout.firstTarget), DataBytes, 0, 1);
    Packet consumer{layout.secondCommands, static_cast<std::uint32_t>(compute.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &consumer) == 0, "compute submission failed");
    std::atomic_ref<std::uint32_t>(*title).store(Done, std::memory_order_release);
    release(first);
    waitUntil([&] { return equal(layout.secondTarget, read); }, "the compute queue did not copy after the label the graphics queue released");
    flip();
    release(second);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const auto capture = checkCapture(1);
    const FrameCaptureReader reader(capture);
    const auto delta = [&](const std::uint32_t* address) {
        return std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) {
            if (chunk.kind != FrameMemoryKind::Delta || at(address) < chunk.address || chunk.address + chunk.bytes < at(address) + 4) return false;
            const auto bytes = reader.Read(chunk);
            std::uint32_t value = 0;
            std::memcpy(&value, bytes.data() + (at(address) - chunk.address), sizeof(value));
            return value == Done;
        });
    };
    check(delta(title), "the memory delta lost the word the title wrote beside the label");
    check(!delta(released), "the memory delta holds the label the graphics queue's RELEASE_MEM wrote, so the replay releases the compute queue before the graphics queue copies");
    check(replayed(tool, capture, layout.secondTarget, DataBytes) == read, "the replay released the compute queue before the copy ahead of the label it waits on");
}

void testFillRewrittenByTitle(const std::filesystem::path& tool) {
    constexpr std::uint32_t Filled = 0x5a5a5a5au;
    constexpr std::uint32_t Stored = 0x77u;
    constexpr std::size_t FillBytes = 512u * 1024u;
    const auto layout = allocate();
    const auto bulk = allocate();
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base + FillBytes / 2);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto graphics = commands(layout.firstCommands);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(bulk.base), 2, 0, Filled, FillBytes, 0, 1, 0);
    writeDone(graphics, layout.label);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(*word == Filled, "the driver did not fill the buffer");
    std::atomic_ref<std::uint32_t>(*word).store(Stored, std::memory_order_release);
    flip();
    std::atomic_ref<std::uint32_t>(*word).store(Filled, std::memory_order_release);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const auto replayedWord = replayed(tool, checkCapture(0), word, 4);
    std::uint32_t value = 0;
    std::memcpy(&value, replayedWord.data(), sizeof(value));
    check(value == Filled, "the replay kept the title's earlier store over its later one, which wrote the value the fill had left");
}

void testFenceReused(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto copied = pattern(53);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    auto* const fence = layout.label + 1;
    const auto gatedCopy = [&](std::uint32_t* words, std::byte* target, std::uint32_t* label) {
        auto buffer = commands(words);
        sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, fence, Done, 0xffffffffu, 0);
        sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(target), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
        writeDone(buffer, label);
        Packet packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
        check(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submission failed");
        std::atomic_ref<std::uint32_t>(*fence).store(Done, std::memory_order_release);
        waitUntil([&] { return std::atomic_ref<std::uint32_t>(*label).load(std::memory_order_acquire) == Done; }, "the driver did not run the copy behind the fence");
        std::atomic_ref<std::uint32_t>(*fence).store(0, std::memory_order_release);
    };

    flip();
    gatedCopy(layout.firstCommands, layout.firstTarget, layout.label);
    flip();
    gatedCopy(layout.secondCommands, layout.secondTarget, layout.label + 2);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(0), layout.firstTarget, 2 * DataBytes) == [&] {
        auto both = copied;
        both.insert(both.end(), copied.begin(), copied.end());
        return both;
    }(), "the replay did not run both copies behind the fence the title released and re-armed between recorded calls");
}

void testQueueBusyPastPacing(const std::filesystem::path& tool) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    constexpr std::size_t CopyBytes = 64u << 20;
    const auto layout = allocate();
    const auto big = allocate(2 * CopyBytes);
    const auto copied = pattern(29);
    std::memcpy(layout.source, copied.data(), copied.size());
    for (std::size_t index = 0; index < CopyBytes; ++index) big.base[index] = static_cast<std::byte>((index * 13 + 5) & 0xff);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const fence = layout.label + 1;
    auto graphics = commands(layout.firstCommands);
    sceAgcDcbWaitRegMem(&graphics, 0, 3, 0, 0, fence, Done, 0xffffffffu, 0);
    for (std::size_t offset = 0; offset < CopyBytes; offset += CopyBytes / 8) sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(big.base + CopyBytes + offset), DmaMemory, 0, at(big.base + offset), CopyBytes / 8, 0, 1, 0);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    releaseMem(graphics, layout.label);
    Packet producer{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&producer) == 0, "graphics submission failed");
    auto compute = commands(layout.secondCommands);
    sceAgcAcbWaitRegMem(&compute, 0, 3, 0, layout.label, Done, 0xffffffffu, 0);
    sceAgcAcbDmaData(&compute, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.secondTarget), DataBytes, 0, 1);
    Packet consumer{layout.secondCommands, static_cast<std::uint32_t>(compute.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &consumer) == 0, "compute submission failed");
    std::atomic_ref<std::uint32_t>(*fence).store(Done, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not copy behind the label");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(0), layout.firstTarget, DataBytes, " --pace-timeout-ms 1") == copied, "the replay gave up pacing or released the compute queue while the graphics queue was still copying");
}

void testFillClearedBeforeRange(const std::filesystem::path& tool) {
    constexpr std::uint32_t Filled = 0x5a5a5a5au;
    const auto layout = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
    check(block != nullptr, "cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04, BlockBytes);
    auto graphics = commands(layout.firstCommands);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(block), 2, 0, Filled, BlockBytes, 0, 1, 0);
    writeDone(graphics, layout.label);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(*reinterpret_cast<const std::uint32_t*>(block) == Filled, "the driver did not fill the block");
    std::memset(block, 0, BlockBytes);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, BlockBytes, true, true, true);
    }
    copy(layout.secondCommands, CommandBytes, block, layout.firstTarget, layout.label);
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(replayed(tool, checkCapture(0), layout.firstTarget, DataBytes) == std::vector<std::byte>(DataBytes), "the replay copied the GPU fill of a block the title cleared before its range was first recorded");
}

}

int main(int argc, char** argv) {
    const std::string mode = argc == 3 ? argv[2] : "";
    const std::map<std::string, void (*)(const std::filesystem::path&)> rewound{
        {"rewind_store", testStoreAfterGatedCopy}, {"rewind_copy", testCopyInFlightAtDelta}, {"rewind_compute", testComputeWaitBehindRewind}, {"rewind_release", testReleaseDuringDelta}, {"rewind_fill", testFilledRewindTail}, {"rewind_split", testRewindFilledAcrossChunks}, {"wait_label", testLabelFromLaterSubmission}, {"gpu_store", testGpuStoreBehindRewind}, {"gpu_fill", testFillRewrittenByTitle}, {"gpu_cleared", testFillClearedBeforeRange}, {"fence_reuse", testFenceReused}, {"busy_queue", testQueueBusyPastPacing}};
    if (argc != 2 && !(argc == 3 && (mode == "concurrent" || mode == "pipelines" || mode == "reservation_error" || mode == "rewind" || rewound.contains(mode)))) {
        std::fprintf(stderr, "usage: agc_driver_frame_replay_tests <agc_frame_replay> [concurrent|pipelines|reservation_error|rewind|rewind_store|rewind_copy|rewind_compute|rewind_release|rewind_fill|rewind_split|wait_label|gpu_store|gpu_fill|gpu_cleared|fence_reuse|busy_queue]\n");
        return 2;
    }
    int status = 0;
    try {
        if (const auto test = rewound.find(mode); test != rewound.end()) {
            try {
                AgcDriver::DriverDetail::DriverTestAccess::OpenDevice();
            } catch (const std::exception& error) {
                if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
                throw Inconclusive(std::string("no usable Vulkan device: ") + error.what());
            }
            test->second(argv[1]);
        } else if (mode == "concurrent") testConcurrentCopies(argv[1]);
        else if (mode == "reservation_error") testReservationError(argv[1]);
        else if (mode == "rewind") testParkedRewindReplays(argv[1]);
        else testCaptureAndReplay(argv[1], mode == "pipelines");
    } catch (const Inconclusive& error) {
        std::printf("skipped, %s\n", error.what());
        status = 77;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        status = 1;
    }
    try {
        AgcDriverShutdown_nid_postfix();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED shutdown: %s\n", error.what());
        return 1;
    }
    if (status != 0) return status;
    std::puts("frame replay tests passed");
    return 0;
}
