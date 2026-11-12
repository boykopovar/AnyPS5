#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "execution/VulkanTestDevice.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
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
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace AgcDriver::DriverDetail {
struct DriverTestAccess {
    static void OpenDevice() {
        auto& driver = Driver::Get();
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (driver.device == nullptr) driver.device = std::make_shared<VulkanDevice>();
    }
    static void KeepLabelRecord(std::uint64_t serial, const std::uint32_t* label, std::uint32_t value) {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        driver.unfinishedLabels[serial] = {0u, {LabelValue{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(label)), value, true}}};
    }
    static void ForgetLabelRecord(std::uint64_t serial) {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        driver.unfinishedLabels.erase(serial);
    }
    static std::uint32_t IdleWaiters() {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        return driver.idleWaiters;
    }
    static void NoteLabelStore(const void* address, std::span<const std::byte> bytes) {
        Driver::Get().noteLabelStore(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(address)), bytes, 0, 0);
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

Layout allocate(std::size_t bytes = BlockBytes, std::size_t registrationBytes = 0) {
    auto* const block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    check(block != nullptr, "cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, 0x04, bytes);
#ifdef __linux__
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    {
        GuestAllocations::Mutation mutation;
        const auto piece = registrationBytes == 0 ? bytes : registrationBytes;
        check(piece != 0 && bytes % piece == 0, "invalid guest registration size");
        for (std::size_t offset = 0; offset < bytes; offset += piece) mutation.Add(block + offset, piece, true, true, true);
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
            check(call.completed >= base + submits || (call.flags & AgcDriver::InFlightDeltaFlag) != 0, "a memory delta was read with " + std::to_string(base + submits - call.completed) + " of the " + std::to_string(submits) + " submissions recorded before it unfinished, and is not marked as read in flight");
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

bool parkedOn(const volatile void* address) {
    std::array<AgcDriver::AwaitedWait, 8> waits{};
    const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
    return std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(address); });
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

void checkCallSerials(const FrameCaptureReader& reader, bool flipWithoutSerial) {
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::Submit || call.kind == FrameCallKind::Suspend) check(call.values == std::vector<std::uint64_t>{call.accepted + 1}, "the capture did not record the serial that call " + std::to_string(call.ordinal) + " took: " + reader.Header());
        if (call.kind == FrameCallKind::Flip) check(call.values.size() == 5 && (call.values.back() == 0 || call.values.back() == call.accepted + 1), "the capture did not record the serial that flip " + std::to_string(call.ordinal) + " took, or 0 for none: " + reader.Header());
    }
    if (flipWithoutSerial) check(std::any_of(reader.Calls().begin(), reader.Calls().end(), [](const auto& call) { return call.kind == FrameCallKind::Flip && call.values.size() == 5 && call.values.back() == 0; }), "the capture did not record 0 for the CPU flip that took no serial: " + reader.Header());
}

void checkTakenSerials(const FrameCaptureReader& reader, std::size_t flips, std::size_t suspends) {
    checkCallSerials(reader, false);
    const auto& calls = reader.Calls();
    for (const auto& call : calls) {
        if (call.kind == FrameCallKind::Flip) check(call.values.back() == call.accepted + 1, "the capture did not record the serial that flip " + std::to_string(call.ordinal) + " took: " + reader.Header());
    }
    const auto count = [&](FrameCallKind kind) { return static_cast<std::size_t>(std::count_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == kind; })); };
    check(count(FrameCallKind::Flip) == flips && count(FrameCallKind::Suspend) == suspends, "the capture's counts of flips and suspend points are not " + std::to_string(flips) + " and " + std::to_string(suspends) + ": " + reader.Header());
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

void checkStoreAfterCopyBehindIdleWait(const std::filesystem::path& tool, bool rewound) {
    const auto layout = allocate();
    const auto copied = pattern(7);
    const auto stored = pattern(211);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    const auto gatedCopy = [&](CommandBuffer& buffer) {
        sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
        writeDone(buffer, layout.label);
    };
    if (rewound) {
        release(submitRewound(layout.firstCommands, [](CommandBuffer&) {}, gatedCopy));
    } else {
        auto* const gate = layout.label + 1;
        auto buffer = commands(layout.firstCommands);
        sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
        gatedCopy(buffer);
        Packet packet{layout.firstCommands, static_cast<std::uint32_t>(buffer.cursor_up - layout.firstCommands), 0, {}};
        check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
        std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    }
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the gated copy did not run");
    std::memcpy(layout.firstTarget, stored.data(), stored.size());
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not finish the gated copy before the CPU store");
    check(replayed(tool, checkCapture(0), layout.firstTarget, DataBytes, " --no-release") == stored, rewound ? "the replay applied the CPU store before the copy in the tail of the REWIND that preceded it" : "the replay applied the CPU store before the gated copy that preceded it");
}

void testStoreAfterGatedCopyIdle(const std::filesystem::path& tool) {
    checkStoreAfterCopyBehindIdleWait(tool, false);
}

void testStoreAfterRewoundCopyIdle(const std::filesystem::path& tool) {
    checkStoreAfterCopyBehindIdleWait(tool, true);
}

void testStoreBeforeSecondWaitOnGate(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto copied = pattern(7);
    const auto stored = pattern(211);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const gate = layout.label + 1;
    auto buffer = commands(layout.firstCommands);
    sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
    sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    writeDone(buffer, layout.label);
    sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, gate, Done + 1, 0xffffffffu, 0);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(buffer.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the gated copy did not run");
    std::memcpy(layout.firstTarget, stored.data(), stored.size());
    flip();
    std::atomic_ref<std::uint32_t>(*gate).store(Done + 1, std::memory_order_release);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not finish the gated copy before the CPU store");
    check(replayed(tool, checkCapture(1), layout.firstTarget, DataBytes, " --no-release") == stored, "the replay ran the copy behind the gate's first wait after restoring the CPU store the capture read while the queue waited on the gate's second value");
}

void testUnwatchedStoreBeforeReservation(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto copied = pattern(7);
    const auto stored = pattern(211);
    std::memcpy(layout.source, copied.data(), copied.size());
    AgcDriver::GuestMemory::Unwatch(at(layout.firstTarget), DataBytes);
    check(!AgcDriver::GuestMemory::Watched(at(layout.firstTarget), DataBytes), "the copy target did not lose write-watch coverage");
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    auto* const gate = layout.label + 1;
    auto gated = commands(layout.firstCommands);
    sceAgcDcbWaitRegMem(&gated, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
    sceAgcDcbDmaData(&gated, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    writeDone(gated, layout.label);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(gated.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
    std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(*layout.label).load() == Done; }, "the gated copy did not run");
    std::memcpy(layout.firstTarget, stored.data(), stored.size());
    const auto token = AgcDriverReserveEopFlip_nid_postfix(AgcDriver::FlipInfo{Handle, -1, 1, 0});
    check(token != 0, "the EOP flip was not reserved");
    auto reading = commands(layout.secondCommands);
    sceAgcDcbDmaData(&reading, 0, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(layout.firstTarget), DataBytes, 0, 1, 0);
    const auto flipped = at(layout.label + 2);
    const std::array<std::uint32_t, 8> eop{0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(flipped), static_cast<std::uint32_t>(flipped >> 32u), Done, 0u, token};
    std::memcpy(reading.cursor_up, eop.data(), sizeof(eop));
    reading.cursor_up += eop.size();
    Packet flipping{layout.secondCommands, static_cast<std::uint32_t>(reading.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&flipping) == 0, "the flipping submission failed");
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.secondTarget, stored), "the driver did not copy the CPU store");
    check(replayed(tool, checkCapture(0), layout.secondTarget, DataBytes, " --no-release") == stored, "the replay copied the gated copy's bytes, not the CPU store recorded before the EOP flip reservation");
}

void testCopyInFlightAtDelta(const std::filesystem::path& tool) {
    constexpr std::size_t CopyBytes = 128u << 20;
    constexpr std::size_t ChunkBytes = CopyBytes / 8;
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
        for (std::size_t offset = 0; offset < CopyBytes - Stored; offset += ChunkBytes) {
            const auto bytes = std::min(ChunkBytes, CopyBytes - Stored - offset);
            sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(target + Stored + offset), DmaMemory, 0, at(source + offset), bytes, 0, 1, 0);
        }
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
    waitUntil([&] { return parkedOn(awaited); }, "the compute wait never reached the awaited-wait registry");
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
    bool held = false;
    std::thread opener([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!(held = parkedOn(gate) && AgcDriverSettledHeld_nid_postfix()) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    });
    flip();
    opener.join();
    check(held, "the recorder never held the queue parked on its gate while it read the memory delta");
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

void checkUnwatchedTitleStores(const std::filesystem::path& tool, bool initiallyUnwatched, const std::function<void(std::byte*)>& prepare = {}) {
    constexpr std::uint32_t Stored = 0x77u;
    const auto bulk = allocate();
    check(AgcDriver::GuestMemory::Watched(at(bulk.base), 4096), "the fixture needs initially watched memory");
    if (prepare) prepare(bulk.base);
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    std::atomic_ref<std::uint32_t>(*word).store(0, std::memory_order_release);
    if (initiallyUnwatched) AgcDriver::GuestMemory::Unwatch(at(bulk.base), 4096);
    flip();
    if (!initiallyUnwatched) AgcDriver::GuestMemory::Unwatch(at(bulk.base), 4096);
    check(!AgcDriver::GuestMemory::Watched(at(bulk.base), 4096), "the test buffer did not lose write-watch coverage");
    std::atomic_ref<std::uint32_t>(*word).store(Stored, std::memory_order_release);
    flip();
    std::atomic_ref<std::uint32_t>(*word).store(0, std::memory_order_release);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const auto capture = checkCapture(0);
    const FrameCaptureReader reader(capture);
    if (prepare) {
        check(std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) {
            return chunk.kind == FrameMemoryKind::Range && chunk.rangeKind == AgcDriver::FrameRangeKind::Section && chunk.address <= at(word) && at(word) + 4 <= chunk.address + chunk.bytes;
        }), "the capture fixture did not record the word in shared section memory");
    }
    std::uint64_t storedGroup = 0;
    std::uint64_t restoredGroup = 0;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address > at(word) || at(word) + 4 > chunk.address + chunk.bytes) continue;
        const auto bytes = reader.Read(chunk);
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + (at(word) - chunk.address), sizeof(value));
        if (value == Stored && storedGroup == 0) storedGroup = chunk.group;
        if (value == 0 && storedGroup != 0 && chunk.group > storedGroup) restoredGroup = chunk.group;
    }
    check(storedGroup != 0, "the capture lost the intermediate CPU store into unwatched memory");
    check(restoredGroup > storedGroup, "the capture lost the CPU restore to its initial value in unwatched memory");
    const auto dumped = replayed(tool, capture, word, 4);
    std::uint32_t value = 0;
    std::memcpy(&value, dumped.data(), sizeof(value));
    check(value == 0, "the replay did not restore the unwatched word's initial value");
}

void testInitiallyUnwatchedTitleStores(const std::filesystem::path& tool) {
    checkUnwatchedTitleStores(tool, true);
}

void testLaterUnwatchedTitleStores(const std::filesystem::path& tool) {
    checkUnwatchedTitleStores(tool, false);
}

void testSharedTitleStores(const std::filesystem::path& tool) {
    constexpr std::size_t Bytes = 65536;
#ifdef _WIN32
    struct Backing {
        HANDLE section = nullptr;
        std::byte* mapped = nullptr;
        ~Backing() {
            if (mapped != nullptr) GuestArena::GuestArenaReset_nid_postfix(mapped, 2 * Bytes);
            if (section == nullptr) return;
            GuestArena::GuestArenaForgetSection_nid_postfix(section);
            CloseHandle(section);
        }
    } backing;
    backing.section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, Bytes, nullptr);
    check(backing.section != nullptr, "cannot create the capture fixture's shared backing");
    checkUnwatchedTitleStores(tool, true, [&](std::byte* base) {
        backing.mapped = base;
        AgcDriver::GuestMemory::Unwatch(at(base), 2 * Bytes);
        GuestArena::GuestArenaReset_nid_postfix(base, BlockBytes);
        GuestArena::GuestArenaMapDirect_nid_postfix(base, Bytes, backing.section, 0, PAGE_READWRITE);
        GuestArena::GuestArenaMapDirect_nid_postfix(base + Bytes, Bytes, backing.section, 0, PAGE_READWRITE);
    });
#else
    const auto closeFile = [](std::FILE* file) { std::fclose(file); };
    std::unique_ptr<std::FILE, decltype(closeFile)> backing(std::tmpfile(), closeFile);
    check(backing != nullptr && ftruncate(fileno(backing.get()), Bytes) == 0, "cannot create the capture fixture's shared backing");
    checkUnwatchedTitleStores(tool, true, [&](std::byte* base) {
        AgcDriver::GuestMemory::Unwatch(at(base), 2 * Bytes);
        check(mmap(base, Bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fileno(backing.get()), 0) == base, "cannot map the capture fixture's shared backing");
        check(mmap(base + Bytes, Bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fileno(backing.get()), 0) == base + Bytes, "cannot map the capture fixture's shared alias");
    });
#endif
}

void testGpuFillRestoredToCachedValue(const std::filesystem::path& tool) {
    constexpr std::uint32_t Filled = 0x5a5a5a5au;
    constexpr std::size_t FillBytes = 65536;
    const auto layout = allocate();
    const auto bulk = allocate();
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    flip();
    AgcDriver::GuestMemory::Unwatch(at(bulk.base), FillBytes);
    flip();
    auto graphics = commands(layout.firstCommands);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(bulk.base), 2, 0, Filled, FillBytes, 0, 1, 0);
    writeDone(graphics, layout.label);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(graphics.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "graphics submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(*word == Filled, "the driver did not fill the previously sampled buffer");
    std::memset(bulk.base, 0, FillBytes);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const auto capture = checkCapture(0);
    const char* dump = std::getenv("APS5_DUMP_SUBMITS");
    const bool cpuFlipsUnordered = !APS5_ENABLE_TIMING_LOG && std::getenv("APS5_CPU_FLIP_IN_ORDER") == nullptr && std::getenv("APS5_SYNC_FLIP") == nullptr && (dump == nullptr || *dump == '\0');
    checkCallSerials(FrameCaptureReader(capture), cpuFlipsUnordered);
    const auto dumped = replayed(tool, capture, word, 4);
    std::uint32_t value = 0;
    std::memcpy(&value, dumped.data(), sizeof(value));
    check(value == 0, "the replay lost a CPU restore matching the cached bytes after an intervening GPU fill");
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

void testRegisteredRangeBoundaries(const std::filesystem::path& tool) {
    const auto layout = allocate(2 * BlockBytes, BlockBytes);
    auto* const otherTarget = layout.base + BlockBytes + DataBytes;
    const auto expected = pattern(43);
    std::memcpy(layout.source, expected.data(), expected.size());
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    copyFrame(layout.firstCommands, layout, otherTarget);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    auto back = layout;
    back.source = otherTarget;
    copyFrame(layout.secondCommands, back, layout.firstTarget);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, expected) && equal(otherTarget, expected), "the driver did not copy across the registered boundary in both directions");

    const auto capture = checkCapture(0);
    const FrameCaptureReader reader(capture);
    const auto boundary = at(layout.base + BlockBytes);
    const auto recorded = [&](std::uint64_t address) {
        return std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) {
            return chunk.kind == FrameMemoryKind::Range && chunk.address == address && chunk.bytes == BlockBytes;
        });
    };
    check(recorded(at(layout.base)) && recorded(boundary), "the capture merged adjacent registered ranges with identical access and protection");
    const auto dumped = replayed(tool, capture, layout.base, 2 * BlockBytes);
    check(dumped.size() == 2 * BlockBytes, "the replay dump has the wrong size");
    check(std::equal(expected.begin(), expected.end(), dumped.begin() + (layout.firstTarget - layout.base)) &&
              std::equal(expected.begin(), expected.end(), dumped.begin() + (otherTarget - layout.base)),
          "the replay did not preserve copies across the registered boundary in both directions");
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

void awaitStarted() {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (AgcDriverQueuedSerials_nid_postfix() != 0) {
        check(std::chrono::steady_clock::now() < limit, "the copying submission did not start within 10 s");
        std::this_thread::yield();
    }
}

constexpr std::size_t BusyPackets = 20000;
constexpr std::size_t BusyLabels = 512u * 1024u;

Packet busyPacket(const Layout& busy, const std::uint32_t* first = nullptr) {
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const end = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer buffer{words, end, words, end, nullptr, nullptr, 0};
    if (first != nullptr) writeDone(buffer, first);
    const auto* const labels = reinterpret_cast<const std::uint32_t*>(busy.base + BusyLabels);
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(buffer, labels + index);
    return Packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
}

Packet labelPacket(std::uint32_t* words, const std::uint32_t* label) {
    auto buffer = commands(words);
    writeDone(buffer, label);
    return Packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
}

void settleDeltas(const Packet& warming) {
    for (int pass = 0; pass < 2; ++pass) {
        check(sceAgcDriverSubmitDcb(&warming) == 0, "the warming submission failed");
        AgcDriverWaitIdle_nid_postfix();
    }
}

const AgcDriver::FrameCall& submitted(const FrameCaptureReader& reader, const std::uint32_t* words) {
    const auto& calls = reader.Calls();
    const auto submit = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(words); });
    check(submit != calls.end(), "the capture lost a submission of the test: " + reader.Header());
    return *submit;
}

const AgcDriver::FrameCall& deltaReadInFlight(const FrameCaptureReader& reader, const std::uint32_t* behind) {
    const auto& calls = reader.Calls();
    const auto& submit = submitted(reader, behind);
    if (submit.accepted == submit.completed) throw Inconclusive("the busy submission finished before the title's next call, so no delta was read while it ran");
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == submit.ordinal; });
    check(delta != calls.end(), "the capture recorded no memory delta for the page the title wrote while the busy submission ran: " + reader.Header());
    check((delta->flags & AgcDriver::InFlightDeltaFlag) != 0, "the delta read while the busy submission ran was held, not read in flight: " + reader.Header());
    return *delta;
}

void testDeltaReadInFlight(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto running = busyPacket(busy);
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(written.source), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(41);
    std::memcpy(written.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not copy the title's store");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    deltaReadInFlight(reader, layout.secondCommands);
    check(reader.Header().find("\"read_depths\":[0,1,0,") != std::string::npos, "the capture header does not count the one delta read in flight behind one running submission: " + reader.Header());
    checkTakenSerials(reader, 2, 1);
    check(replayed(tool, checkCapture(1), layout.firstTarget, DataBytes) == stored, "the replay did not copy the page the title stored while the busy submission ran");
}

std::vector<std::uint64_t> deltaGroupsHolding(const FrameCaptureReader& reader, const std::byte* page) {
    std::vector<std::uint64_t> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address >= at(page) + DataBytes || at(page) >= chunk.address + chunk.bytes) continue;
        if (std::find(groups.begin(), groups.end(), chunk.group) == groups.end()) groups.push_back(chunk.group);
    }
    std::sort(groups.begin(), groups.end());
    return groups;
}

std::string groupList(const std::vector<std::uint64_t>& groups) {
    std::string text;
    for (const auto group : groups) text += (text.empty() ? "" : " ") + std::to_string(group);
    return text.empty() ? "none" : text;
}

void testDeltaReadInFlightOnce(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &busy}) std::memset(touched->base, 0, BlockBytes);
    auto* const later = written.base + 4 * AgcDriver::FrameCaptureWriter::BlockBytes;
    const auto running = busyPacket(busy);
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(written.source), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    auto third = commands(written.firstCommands);
    sceAgcDcbDmaData(&third, 0, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(later), DataBytes, 0, 1, 0);
    const Packet readingLater{written.firstCommands, static_cast<std::uint32_t>(third.cursor_up - written.firstCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0 && sceAgcDriverSubmitDcb(&readingLater) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(47);
    std::memcpy(written.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not copy the title's store");
    const auto storedLater = pattern(53);
    std::memcpy(later, storedLater.data(), storedLater.size());
    check(sceAgcDriverSubmitDcb(&readingLater) == 0, "the submission copying the later page failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.secondTarget, storedLater), "the driver did not copy the title's later store");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto group = deltaReadInFlight(reader, layout.secondCommands).values.at(0);
    const auto holding = deltaGroupsHolding(reader, written.source);
    check(holding == std::vector<std::uint64_t>{group}, "the page the title stored once, before the delta read while the busy submission ran, is in memory delta groups " + groupList(holding) + ", not only in group " + std::to_string(group) + ", so the read in flight did not move its range past the page and a later delta restored it again although nothing stored into it after that read: " + header);
    const auto& calls = reader.Calls();
    const auto& submit = submitted(reader, written.firstCommands);
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == submit.ordinal; });
    check(delta != calls.end() && delta->accepted == delta->completed && (delta->flags & AgcDriver::InFlightDeltaFlag) == 0, "the capture recorded no memory delta read with nothing in flight for the page the title stored after the driver went idle: " + header);
    const auto holdingLater = deltaGroupsHolding(reader, later);
    check(holdingLater == std::vector<std::uint64_t>{delta->values.at(0)}, "the page the title stored after the delta read in flight is in memory delta groups " + groupList(holdingLater) + ", not only in group " + std::to_string(delta->values.at(0)) + " read before the call that copies it, so a later delta restored it again although nothing stored into it after that read: " + header);
    check(header.find("\"stale_delta\":0,\"unverified_deltas\":0,") != std::string::npos, "the capture marked a delta stale or unverified although no submission stored into a page read while the busy submission ran: " + header);
    auto both = stored;
    both.insert(both.end(), storedLater.begin(), storedLater.end());
    check(replayed(tool, checkCapture(1), layout.firstTarget, 2 * DataBytes) == both, "the replay did not copy the page the title stored before the delta read in flight and the page it stored after that delta");
}

void testFlipSerialAtStart(const std::filesystem::path& tool) {
    if (APS5_ENABLE_TIMING_LOG) throw Inconclusive("a build with the timing log orders every CPU flip, so no CPU flip of the test can take the path without a serial");
    const char* submitDump = std::getenv("APS5_DUMP_SUBMITS");
    check(std::getenv("APS5_CPU_FLIP_IN_ORDER") == nullptr && std::getenv("APS5_SYNC_FLIP") == nullptr && (submitDump == nullptr || *submitDump == '\0'), "the environment orders CPU flips or captures submissions, so no CPU flip of the test can take the path without a serial");
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto running = busyPacket(busy);
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(written.source), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    std::async(std::launch::async, [&] {
        check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0, "the submissions warming the imports failed");
        AgcDriverWaitIdle_nid_postfix();
    }).get();

    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(43);
    std::memcpy(written.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not copy the title's store");
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const FrameCaptureReader reader(capture);
    check(!reader.Truncated() && reader.Header().find("stopped") == std::string::npos && reader.Header().find("\"frames\":2") != std::string::npos, "the capture of two frames did not end normally: " + reader.Header());
    const auto& calls = reader.Calls();
    const auto start = std::find_if(calls.begin(), calls.end(), [](const auto& call) { return call.kind == FrameCallKind::FrameStart; });
    check(start != calls.end() && start->accepted != 0, "the capture did not start after the driver had accepted the submissions that warmed the imports: " + reader.Header());
    const auto first = std::find_if(start, calls.end(), [](const auto& call) { return call.kind == FrameCallKind::Flip; });
    check(first != calls.end() && first->values.size() == 5 && first->values.back() == 0, "the first CPU flip after the frame start, from a thread without graphics submissions, took a serial in the capture: " + reader.Header());
    deltaReadInFlight(reader, layout.secondCommands);
    checkCallSerials(reader, true);
    const auto serialFlips = static_cast<std::size_t>(std::count_if(start, calls.end(), [](const auto& call) { return call.kind == FrameCallKind::Flip && call.values.size() == 5 && call.values.back() != 0; }));
    const auto dump = capture / "dump.bin";
    const auto log = capture / "replay.log";
#ifdef _WIN32
    _putenv_s("APS5_TRACE_GPU", "1");
#else
    setenv("APS5_TRACE_GPU", "1", 1);
#endif
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --dump " + std::to_string(at(layout.firstTarget)) + ":" + std::to_string(DataBytes) + ":\"" + dump.string() + "\" > \"" + (capture / "replay.json").string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    const auto logged = readFile(log);
    const std::string text(reinterpret_cast<const char*>(logged.data()), logged.size());
    const auto refusal = text.find("agc_frame_replay: ");
    check(code == 0, "agc_frame_replay exited with " + std::to_string(code) + (refusal == std::string::npos ? std::string() : ": " + text.substr(refusal, text.find('\n', refusal) - refusal)));
    check(readFile(dump) == stored, "the replay did not copy the page the title stored while the busy submission ran");
    std::size_t ordered = 0;
    for (auto found = text.find(" (cpu flip)"); found != std::string::npos; found = text.find(" (cpu flip)", found + 1)) ++ordered;
    check(ordered == serialFlips, "the replay's CPU flips took " + std::to_string(ordered) + " serials, by the driver's accepted-serial trace, where the capture's took " + std::to_string(serialFlips) + ", so the replay did not start with an accepted serial as the capture's frame start did");
}

void testStoreIntoDeltaReadInFlight(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto page = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &page, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto copied = pattern(23);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer graphics{words, labels, words, labels, nullptr, nullptr, 0};
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(graphics, labels + index);
    const Packet running{words, static_cast<std::uint32_t>(graphics.cursor_up - words), 0, {}};
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(page.source), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    const Packet copying{words, static_cast<std::uint32_t>(graphics.cursor_up - words), 0, {}};
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(page.source), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&copying) == 0, "the copying submission failed");
    awaitStarted();
    const auto stored = pattern(57);
    std::memcpy(page.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    if (equal(page.source, stored)) throw Inconclusive("the copy behind the busy labels stored into the page before the title's store, so no delta read while it ran could miss that store");
    check(equal(page.source, copied) && equal(layout.secondTarget, copied), "the driver did not run the copy behind the busy labels before the submission that reads its target");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto group = deltaReadInFlight(reader, layout.secondCommands).values.at(0);
    char stalePage[32];
    std::snprintf(stalePage, sizeof stalePage, "%llx", static_cast<unsigned long long>(at(page.source)));
    const auto stopped = "stopped: memory delta " + std::to_string(group) + " was read while submission " + std::to_string(submitted(reader, words).accepted + 1) + ", in flight at its read, stored into its page 0x" + stalePage;
    check(header.find("\"stale_delta\":" + std::to_string(group) + ",") != std::string::npos && header.find(stopped) != std::string::npos, "the copy behind the busy labels stored into the title's page of a delta read while it ran, and the capture did not stop on that page: " + header);
    const auto log = std::filesystem::path(directory) / "refused.log";
    auto command = "\"" + tool.string() + "\" \"" + std::string(directory) + "\" > \"" + (std::filesystem::path(directory) / "refused.json").string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    check(std::system(command.c_str()) != 0, "the replay accepted a capture with a stale delta");
    const auto errors = readFile(log);
    const std::string refusal(reinterpret_cast<const char*>(errors.data()), errors.size());
    check(refusal.find("the capture stopped because a memory delta was read while submissions that stored into it were in flight (stale_delta " + std::to_string(group) + ")") != std::string::npos, "the replay did not refuse the stale capture while it restored memory, before it made its device: " + refusal);
}

void testEmptyDeltaReadInFlight(const std::filesystem::path&) {
    const auto layout = allocate();
    const auto page = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &page, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto copied = pattern(23);
    std::memcpy(layout.source, copied.data(), copied.size());
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer graphics{words, labels, words, labels, nullptr, nullptr, 0};
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(graphics, labels + index);
    const Packet running{words, static_cast<std::uint32_t>(graphics.cursor_up - words), 0, {}};
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(page.source), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    const Packet copying{words, static_cast<std::uint32_t>(graphics.cursor_up - words), 0, {}};
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.secondTarget), DmaMemory, 0, at(page.source), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&copying) == 0, "the copying submission failed");
    awaitStarted();
    check(AgcDriver::GuestMemory::Watched(at(page.source), DataBytes), "the page the title stores into is not write-watched, so the delta at its next call would list the whole unwatched block around it rather than the one page whose every word the GPU stored");
    const auto stored = pattern(61);
    AgcDriver::DriverDetail::DriverTestAccess::NoteLabelStore(page.source, stored);
    std::memcpy(page.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    if (equal(page.source, stored)) throw Inconclusive("the copy behind the busy labels stored into the page before the title's store, so no delta read while it ran could miss that store");
    check(equal(page.source, copied) && equal(layout.secondTarget, copied), "the driver did not run the copy behind the busy labels before the submission that reads its target");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& submit = submitted(reader, layout.secondCommands);
    if (submit.accepted == submit.completed) throw Inconclusive("the busy submission finished before the title's next call, so no delta was read while it ran");
    const std::string staleKey = "\"stale_delta\":";
    const auto staleAt = header.find(staleKey);
    check(staleAt != std::string::npos, "the capture header has no stale_delta: " + header);
    const auto stale = std::stoull(header.substr(staleAt + staleKey.size()));
    char stalePage[32];
    std::snprintf(stalePage, sizeof stalePage, "%llx", static_cast<unsigned long long>(at(page.source)));
    const auto stopped = "stopped: memory delta " + std::to_string(stale) + " was read while submission " + std::to_string(submitted(reader, words).accepted + 1) + ", in flight at its read, stored into its page 0x" + stalePage;
    check(stale != 0 && header.find(stopped) != std::string::npos, "the copy behind the busy labels stored into the page of the delta read while it ran, and the capture did not stop on that page naming a delta and the copying submission, so the stale read goes unnoticed and a replay accepts the capture: " + header);
    const auto& calls = reader.Calls();
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && !call.values.empty() && call.values.front() == stale; });
    check(delta != calls.end(), "the capture stopped on memory delta " + std::to_string(stale) + " but holds no memory call of that group, so the delta read while the busy submission ran, which wrote no bytes because every word of its page was one the GPU stored, was dropped and the stale page it owned names a delta the capture does not have: " + header);
    check((delta->flags & AgcDriver::InFlightDeltaFlag) != 0 && delta->ordinal + 1 == submit.ordinal, "memory delta " + std::to_string(stale) + " is not the delta read in flight at the title's call after its store into the page: " + header);
    check(std::none_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) { return chunk.kind == FrameMemoryKind::Delta && chunk.group == stale; }), "the delta read while the busy submission ran wrote bytes although every word of the page the title stored was one the GPU had stored, so the test did not reach a delta read in flight that writes nothing: " + header);
}

void checkOwnerLifetime(const std::filesystem::path& tool, bool rewritten) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    const auto other = allocate();
    for (const auto* touched : {&layout, &written, &busy, &other}) std::memset(touched->base, 0, BlockBytes);
    auto* const page = written.source;
    auto* const elsewhere = other.source;
    const auto running = busyPacket(busy);
    auto second = commands(layout.secondCommands);
    sceAgcDcbDmaData(&second, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(page), DataBytes, 0, 1, 0);
    const Packet reading{layout.secondCommands, static_cast<std::uint32_t>(second.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    const auto closing = labelPacket(other.firstCommands, other.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&reading) == 0 && sceAgcDriverSubmitDcb(&closing) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(89);
    std::memcpy(page, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&reading) == 0, "the reading submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not copy the title's store");
    const auto again = pattern(109);
    if (rewritten) std::memcpy(page, again.data(), again.size());
    const auto unrelated = pattern(113);
    std::memcpy(elsewhere, unrelated.data(), unrelated.size());
    check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission after the busy one completed failed");
    AgcDriverWaitIdle_nid_postfix();
    check(AgcDriver::GuestMemory::DriverStoreObserved(), "the frame recorder did not observe the driver's stores before the store without a serial, so that store could not reach it");
    {
        const AgcDriver::GuestMemory::StoreSerialScope unattributed(0);
        AgcDriver::GuestMemory::MarkWritten(at(page), sizeof(std::uint32_t));
    }
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    const auto inFlight = deltaReadInFlight(reader, layout.secondCommands).values.at(0);
    const auto& closed = submitted(reader, other.firstCommands);
    check(closed.accepted == closed.completed, "the submission after the busy one completed was recorded with work in flight, so the delta before it was not read with nothing in flight: " + header);
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == closed.ordinal; });
    check(delta != calls.end() && (delta->flags & AgcDriver::InFlightDeltaFlag) == 0, "the capture recorded no delta read with nothing in flight before the submission after the busy one completed, although the title wrote a page of another block before that submission: " + header);
    const auto settled = delta->values.at(0);
    const auto holds = [&](std::uint64_t group, const std::byte* bytes) {
        return std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) { return chunk.kind == FrameMemoryKind::Delta && chunk.group == group && chunk.address < at(bytes) + DataBytes && at(bytes) < chunk.address + chunk.bytes; });
    };
    check(holds(inFlight, page), "the delta read while the busy submission ran does not hold the page the title stored before it, so no page of the test was read in flight: " + header);
    check(holds(settled, elsewhere), "the delta read with nothing in flight does not hold the page the title wrote after the busy submission completed: " + header);
    if (!rewritten) {
        check(!holds(settled, page), "the delta read with nothing in flight listed the page that the delta read while the busy submission ran had recorded, although the title did not store into that page again: " + header);
        char stalePage[32];
        std::snprintf(stalePage, sizeof stalePage, "%llx", static_cast<unsigned long long>(at(page)));
        const auto stopped = "stopped: memory delta " + std::to_string(inFlight) + " was read while submission (unknown), in flight at its read, stored into its page 0x" + stalePage;
        check(header.find("\"stale_delta\":" + std::to_string(inFlight) + ",") != std::string::npos && header.find(stopped) != std::string::npos, "a driver store without a serial into the page of the delta read while the busy submission ran did not stop the capture on that delta, although no delta read with nothing in flight had listed the page since that read, so the capture cannot tell that store from one by the work in flight at the read: " + header);
        return;
    }
    check(holds(settled, page), "the delta read with nothing in flight did not list the page the title stored into again after the busy submission completed: " + header);
    check(header.find("\"stale_delta\":0,") != std::string::npos, "a driver store without a serial into the page stopped the capture on the delta read while the busy submission ran, although a delta read with nothing in flight had recorded the page again after that submission completed, so the page no longer depended on that read: " + header);
    check(replayed(tool, checkCapture(1), layout.firstTarget, DataBytes) == stored, "the replay did not copy the bytes the title stored while the busy submission ran, before its later store into the same page");
}

void testOwnerLifetime(const std::filesystem::path& tool) {
    checkOwnerLifetime(tool, false);
}

void testOwnerLifetimeRewritten(const std::filesystem::path& tool) {
    checkOwnerLifetime(tool, true);
}

void testDeltaGatedOnStartedCopy(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto origin = allocate();
    const auto busy = allocate(2 * BlockBytes);
    std::memset(layout.base, 0, BlockBytes);
    std::memset(origin.base, 0, BlockBytes);
    std::memset(busy.base, 0, 2 * BlockBytes);
    const auto copied = pattern(83);
    const auto overwritten = pattern(197);
    std::memcpy(origin.source, copied.data(), copied.size());
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(busy.base + BlockBytes);
    CommandBuffer graphics{words, labels, words, labels, nullptr, nullptr, 0};
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(graphics, labels + index);
    sceAgcDcbDmaData(&graphics, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(origin.source), DataBytes, 0, 1, 0);
    writeDone(graphics, layout.label);
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(graphics, labels + index);
    const Packet copying{words, static_cast<std::uint32_t>(graphics.cursor_up - words), 0, {}};
    const auto closing = labelPacket(layout.secondCommands, layout.label + 1);
    const auto warming = labelPacket(layout.firstCommands, layout.label + 2);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&copying) == 0 && sceAgcDriverSubmitDcb(&closing) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not run the copy between the busy labels");
    std::memset(layout.firstTarget, 0, DataBytes);
    std::atomic_ref<std::uint32_t>(*layout.label).store(0, std::memory_order_release);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&copying) == 0, "the copying submission failed");
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(*layout.label).load(std::memory_order_acquire) != Done) {
        check(std::chrono::steady_clock::now() < limit, "the copying submission did not write its label within 10 s");
        std::this_thread::yield();
    }
    std::memcpy(origin.source, overwritten.data(), overwritten.size());
    check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission behind the copying one failed");
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not copy the source before the label the title waited for");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& delta = deltaReadInFlight(reader, layout.secondCommands);
    const auto serial = submitted(reader, words).accepted + 1;
    check(std::find(delta.values.begin() + 1, delta.values.end(), serial) != delta.values.end(), "the delta read while the copying submission ran does not list it as started: " + reader.Header());
    check(replayed(tool, checkCapture(1), layout.firstTarget, DataBytes) == copied, "the replay restored the title's later store into the copy's source before the submission in flight at the delta's read copied it");
}

std::string jsonObject(const std::string& text, const std::string& name) {
    const auto key = text.find("\"" + name + "\":{");
    check(key != std::string::npos, "the JSON has no object " + name + ": " + text);
    const auto open = key + name.size() + 4;
    const auto close = text.find('}', open);
    check(close != std::string::npos, "the JSON object " + name + " is not closed: " + text);
    return text.substr(open, close - open);
}

std::map<std::string, std::uint64_t> jsonCounts(const std::string& members) {
    std::map<std::string, std::uint64_t> counts;
    for (std::size_t cursor = 0; cursor < members.size();) {
        const auto colon = members.find("\":", cursor);
        check(members[cursor] == '"' && colon != std::string::npos, "not a list of JSON counts: " + members);
        const auto end = std::min(members.find(',', colon), members.size());
        counts[members.substr(cursor + 1, colon - cursor - 1)] = std::stoull(members.substr(colon + 2, end - colon - 2));
        cursor = end + 1;
    }
    return counts;
}

std::uint64_t jsonCount(const std::map<std::string, std::uint64_t>& counts, const std::string& key, const std::string& text) {
    const auto found = counts.find(key);
    check(found != counts.end(), "the JSON has no count " + key + ": " + text);
    return found->second;
}

bool awaits(const std::uint32_t* word);

void testHoldDiagnostics(const std::filesystem::path& tool) {
    constexpr std::uint32_t Stored = 0x77u;
    constexpr std::uint32_t ComputeQueue = 0x20;
    const auto layout = allocate();
    const auto bulk = allocate();
    const auto busy = allocate();
    std::memset(busy.base, 0, BlockBytes);
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base);
    AgcDriver::GuestMemory::Unwatch(at(bulk.base), 4096);
    check(!AgcDriver::GuestMemory::Watched(at(bulk.base), 4096), "the test buffer did not lose write-watch coverage");
    auto* const busyWords = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const busyLabels = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    auto* const awaitedLabel = busyLabels + BusyPackets;
    CommandBuffer labelling{busyWords, busyLabels, busyWords, busyLabels, nullptr, nullptr, 0};
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(labelling, busyLabels + index);
    writeDone(labelling, awaitedLabel);
    const Packet labelWriter{busyWords, static_cast<std::uint32_t>(labelling.cursor_up - busyWords), 0, {}};
    auto* const waitWords = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels + 2 * CommandBytes);
    auto waiting = commands(waitWords);
    sceAgcAcbWaitRegMem(&waiting, 0, 3, 0, awaitedLabel, Done, 0xffffffffu, 0);
    sceAgcAcbWriteData(&waiting, ComputeMemory, 0, at(awaitedLabel + 1), &Done, 1, 0, 1);
    const Packet labelWaiter{waitWords, static_cast<std::uint32_t>(waiting.cursor_up - waitWords), 0, {}};
    const auto afterLabel = labelPacket(reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels + 3 * CommandBytes), awaitedLabel + 2);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(sceAgcDriverSubmitAcb(ComputeQueue, &labelWaiter) == 0, "the compute submission that waits on a label failed");
    waitUntil([&] { return awaits(awaitedLabel); }, "the compute submission never waited on its label");
    std::memset(layout.source, 0x11, DataBytes);
    check(sceAgcDriverSubmitDcb(&labelWriter) == 0, "the submission that writes the awaited label failed");
    awaitStarted();
    const auto firstPage = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(busyLabels[1024]).load(std::memory_order_acquire) != Done) {
        check(std::chrono::steady_clock::now() < firstPage, "the busy submission did not finish its first page of labels within 10 s");
        std::this_thread::yield();
    }
    std::atomic_ref<std::uint32_t>(busyLabels[0]).store(Stored, std::memory_order_release);
    std::memset(layout.source, 0x22, DataBytes);
    if (!awaits(awaitedLabel) || std::atomic_ref<std::uint32_t>(*awaitedLabel).load(std::memory_order_acquire) == Done) throw Inconclusive("the busy labels reached the awaited label before the title's next call; raise BusyPackets");
    check(sceAgcDriverSubmitDcb(&afterLabel) == 0, "the submission after the label wait failed");
    AgcDriverWaitIdle_nid_postfix();
    auto* const gate = layout.label + 1;
    auto gated = commands(layout.firstCommands);
    sceAgcDcbWaitRegMem(&gated, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
    writeDone(gated, layout.label);
    Packet first{layout.firstCommands, static_cast<std::uint32_t>(gated.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&first) == 0, "the gated submission failed");
    waitUntil([&] {
        std::array<AgcDriver::AwaitedWait, 8> waits{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
        return std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(gate); });
    }, "the gated submission never waited on its gate");
    std::atomic_ref<std::uint32_t>(busyLabels[1]).store(Stored, std::memory_order_release);
    std::atomic_ref<std::uint32_t>(layout.label[3]).store(Stored, std::memory_order_release);
    auto behind = commands(layout.secondCommands);
    writeDone(behind, layout.label + 2);
    Packet queued{layout.secondCommands, static_cast<std::uint32_t>(behind.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&queued) == 0, "the submission behind the gate failed");
    std::atomic_ref<std::uint32_t>(*word).store(Stored, std::memory_order_release);
    check(sceAgcDriverSubmitDcb(&queued) == 0, "the second submission behind the gate failed");
    std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    check(*layout.label == Done && layout.label[2] == Done, "the driver did not run the submissions behind the gate");
    flip();
    AgcDriverWaitIdle_nid_postfix();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const auto capture = checkCapture(2);
    const FrameCaptureReader reader(capture);
    const auto& header = reader.Header();
    const auto reasons = jsonCounts(jsonObject(header, "hold_reasons"));
    check(jsonCount(reasons, "parked", header) >= 4 && jsonCount(reasons, "queued", header) >= 1, "the capture did not count both the parked gate and the submission queued behind it for the delta read while both were in flight: " + header);
    check(jsonCount(reasons, "unwatched", header) == 0, "the capture held a delta for an unwatched block that only the title had written since its last record: " + header);
    const auto masks = jsonCounts(jsonObject(header, "hold_masks"));
    check(std::any_of(masks.begin(), masks.end(), [](const auto& mask) {
        const auto names = "+" + mask.first + "+";
        return names.find("+parked+") != std::string::npos && names.find("+queued+") != std::string::npos;
    }), "no hold mask of the capture holds both the parked gate and the queued submission: " + header);
    const auto entry = header.find("{\"address\":" + std::to_string(at(bulk.base)) + ",\"bytes\":" + std::to_string(AgcDriver::FrameCaptureWriter::BlockBytes) + ",");
    check(entry != std::string::npos, "the capture header does not list the unwatched block: " + header);
    const auto block = jsonCounts(header.substr(entry + 1, header.find('}', entry) - entry - 1));
    check(jsonCount(block, "deltas", header) >= 2 && jsonCount(block, "changed", header) == 1 && jsonCount(block, "driver_stored", header) == 0, "the unwatched block's counters do not show one CPU store and no driver store between the deltas read while the gate was closed: " + header);
    for (const char* name : {"parked_on_title", "driver_page_since_full", "title_wait_unpassed", "wait_dword_in_delta"}) check(reasons.contains(name), std::string("the capture header does not count the hold reason ") + name + ": " + header);
    check(header.find("\"read_depths\":[") != std::string::npos, "the capture header has no read_depths: " + header);
    check(jsonCount(reasons, "title_wait_unpassed", header) >= 1, "the capture did not count the gate the parked submission waits on as an unpassed wait on title memory: " + header);
    const auto anyMask = [&](std::initializer_list<const char*> with, std::initializer_list<const char*> without) {
        return std::any_of(masks.begin(), masks.end(), [&](const auto& mask) {
            const auto names = "+" + mask.first + "+";
            const auto has = [&](const char* name) { return names.find(std::string("+") + name + "+") != std::string::npos; };
            return std::all_of(with.begin(), with.end(), has) && std::none_of(without.begin(), without.end(), has);
        });
    };
    check(anyMask({"parked", "parked_on_title", "title_wait_unpassed"}, {"wait_dword_in_delta"}), "no hold mask counts the queue parked on a dword nothing in flight labels as parked on title memory with an unpassed wait whose dword the delta does not hold: " + header);
    check(anyMask({"parked"}, {"parked_on_title", "title_wait_unpassed"}), "no hold mask counts the queue parked on a label that a submission in flight writes as parked without parked_on_title or title_wait_unpassed: " + header);
    check(anyMask({"driver_page", "driver_page_since_full"}, {}), "no hold mask counts the label page the driver stored into after the last full delta as driver_page_since_full: " + header);
    check(anyMask({"driver_page"}, {"driver_page_since_full"}), "no hold mask counts the label page the driver stored into before the last full delta as driver_page without driver_page_since_full: " + header);
    check(anyMask({"wait_dword_in_delta"}, {}), "no hold mask counts the delta holding the gate's page as holding a dword the parked submission waits on: " + header);

    const auto result = capture / "hold diagnostics.json";
    const auto dump = capture / "hold diagnostics.bin";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --no-release --dump " + std::to_string(at(word)) + ":4:\"" + dump.string() + "\" > \"" + result.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    check(code == 0, "agc_frame_replay exited with " + std::to_string(code));
    check(readFile(dump) == std::vector<std::byte>{std::byte{Stored}, std::byte{0}, std::byte{0}, std::byte{0}}, "the replay did not restore the CPU store into the unwatched block");
    const auto json = readFile(result);
    const std::string line(reinterpret_cast<const char*>(json.data()), json.size());
    for (const char* field : {"\"memory_held_wait_s\":", "\"memory_in_flight_wait_s\":", "\"restore_s\":", "\"in_flight_mean\":"}) check(line.find(field) != std::string::npos, std::string("the replay JSON has no ") + field + " " + line);
    check(line.find("\"in_flight_max\":2,") != std::string::npos, "the replay did not report the two submissions in flight behind the parked gate: " + line);
}

bool deltaHolds(const FrameCaptureReader& reader, std::uint64_t group, const std::byte* page, const std::vector<std::byte>& expected) {
    std::vector<std::byte> captured(expected.size());
    std::transform(expected.begin(), expected.end(), captured.begin(), [](std::byte value) { return ~value; });
    for (const auto& chunk : reader.Chunks()) {
        const auto begin = std::max(at(page), chunk.address);
        const auto end = std::min<std::uint64_t>(at(page) + expected.size(), chunk.address + chunk.bytes);
        if (chunk.kind != FrameMemoryKind::Delta || chunk.group != group || begin >= end) continue;
        const auto data = reader.Read(chunk);
        std::memcpy(captured.data() + (begin - at(page)), data.data() + (begin - chunk.address), static_cast<std::size_t>(end - begin));
    }
    return captured == expected;
}

void testStoreWhileDeltaHeld(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto other = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &other, &busy}) std::memset(touched->base, 0, BlockBytes);
    check(AgcDriver::GuestMemory::Watched(at(written.base), BlockBytes) && AgcDriver::GuestMemory::Watched(at(other.base), BlockBytes), "the blocks the title and the other thread store into are not write-watched, so no collect can tell a store made while a delta was held from one made before it");
    const auto running = busyPacket(busy);
    auto* const queuedWords = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels + 2 * CommandBytes);
    const auto queued = labelPacket(queuedWords, layout.label + 2);
    const auto closing = labelPacket(layout.secondCommands, layout.label + 1);
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    const auto stored = pattern(71);
    const auto concurrent = pattern(89);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&queued) == 0 && sceAgcDriverSubmitDcb(&closing) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    check(sceAgcDriverSubmitDcb(&queued) == 0, "the submission queued behind the busy one failed");
    std::memcpy(written.source, stored.data(), stored.size());
    check(AgcDriver::DriverDetail::DriverTestAccess::IdleWaiters() == 0, "a thread already waited on the driver before the title's call, so the other thread cannot tell the title's wait for the held delta from that wait");
    std::atomic<bool> otherStored{false};
    std::atomic<bool> storedWhileHeld{false};
    {
        std::jthread helper([&](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (AgcDriver::DriverDetail::DriverTestAccess::IdleWaiters() == 0) {
                    std::this_thread::yield();
                    continue;
                }
                std::memcpy(other.source, concurrent.data(), concurrent.size());
                storedWhileHeld.store(AgcDriver::DriverDetail::DriverTestAccess::IdleWaiters() != 0, std::memory_order_release);
                otherStored.store(true, std::memory_order_release);
                return;
            }
        });
        check(sceAgcDriverSubmitDcb(&closing) == 0, "the title's submission behind the queued one failed");
    }
    AgcDriverWaitIdle_nid_postfix();
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    check(!reader.Truncated() && header.find("stopped") == std::string::npos && header.find("\"frames\":2") != std::string::npos, "the capture of two frames did not end normally: " + header);
    const auto& submit = submitted(reader, layout.secondCommands);
    if (submit.accepted - submit.completed < 2) throw Inconclusive("the busy submission finished before the title's next call, so the submission behind it was no longer queued when the delta was taken");
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == submit.ordinal; });
    check(delta != calls.end(), "the capture recorded no memory delta for the page the title stored before its call: " + header);
    if ((delta->flags & AgcDriver::InFlightDeltaFlag) != 0) {
        const auto serial = submitted(reader, queuedWords).accepted + 1;
        check(std::find(delta->values.begin() + 1, delta->values.end(), serial) != delta->values.end(), "the delta taken while submission " + std::to_string(serial) + " was still queued behind the busy one was read in flight instead of held until both had finished: " + header);
        throw Inconclusive("the submission behind the busy one had started, with the busy one's completion still pending, when the title's call took the delta, so the delta was read in flight instead of held");
    }
    check(delta->accepted == delta->completed, "the held delta was read before the submissions in flight at the title's call had finished: " + header);
    if (!otherStored.load(std::memory_order_acquire)) throw Inconclusive("the other thread never saw the title's call wait for the held delta's submissions to finish, so it stored nothing during that wait");
    if (!storedWhileHeld.load(std::memory_order_acquire)) throw Inconclusive("the held delta's submissions finished while the other thread stored into its page, so the store may have landed after the delta read memory");
    const auto group = delta->values.at(0);
    check(deltaHolds(reader, group, written.source, stored), "the held delta does not hold the page the title stored before its call: " + header);
    check(deltaHolds(reader, group, other.source, concurrent), "the held delta does not hold the page another thread stored while the delta waited for the submissions in flight to finish, so a replay applies that store only at a later call instead of with the memory the delta read after its wait: " + header);
    const auto groups = deltaGroupsHolding(reader, other.source);
    std::string listed;
    for (const auto held : groups) listed += " " + std::to_string(held);
    check(groups == std::vector<std::uint64_t>{group}, "the page another thread stored while the held delta waited is in memory delta groups" + listed + ", not only in the held delta's group " + std::to_string(group) + ", although nothing stored into it after the held delta read it: " + header);
    check(replayed(tool, checkCapture(0), other.source, DataBytes) == concurrent, "the replay did not restore the page another thread stored while the held delta waited");
}

void testStoreAfterLastFrame(const std::filesystem::path& tool) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    constexpr std::size_t Labels = 20000;
    constexpr std::size_t Copied = DataBytes / 2;
    const auto layout = allocate();
    const auto stream = allocate();
    const auto page = allocate();
    const auto copied = pattern(29);
    const auto stored = pattern(61);
    std::memcpy(layout.source, copied.data(), copied.size());
    const std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
    std::memcpy(layout.firstCommands, nop.data(), sizeof(nop));
    std::memcpy(layout.secondCommands, nop.data(), sizeof(nop));
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    Packet warm{layout.firstCommands, static_cast<std::uint32_t>(nop.size()), 0, {}};
    for (int pass = 0; pass < 2; ++pass) {
        check(sceAgcDriverSubmitDcb(&warm) == 0, "the empty graphics submission failed");
        AgcDriverWaitIdle_nid_postfix();
    }
    auto* const words = reinterpret_cast<std::uint32_t*>(stream.base);
    CommandBuffer slow{words, words + BlockBytes / sizeof(std::uint32_t), words, words + BlockBytes / sizeof(std::uint32_t), nullptr, nullptr, 0};
    for (std::size_t index = 0; index < Labels; ++index) writeDone(slow, layout.label);
    sceAgcDcbDmaData(&slow, 0, DmaMemory, 0, at(page.source), DmaMemory, 0, at(layout.source), Copied, 0, 1, 0);
    Packet labels{words, static_cast<std::uint32_t>(slow.cursor_up - words), 0, {}};
    check(sceAgcDriverSubmitDcb(&labels) == 0, "the submission of labels followed by a copy failed");
    awaitStarted();
    std::memcpy(page.source + Copied, stored.data() + Copied, DataBytes - Copied);
    Packet empty{layout.secondCommands, static_cast<std::uint32_t>(nop.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &empty) == 0, "the empty compute submission failed");
    awaitStarted();
    flip();
    const bool observed = AgcDriver::GuestMemory::DriverStoreObserved();
    AgcDriverWaitIdle_nid_postfix();
    check(std::memcmp(page.source, copied.data(), Copied) == 0 && std::memcmp(page.source + Copied, stored.data() + Copied, DataBytes - Copied) == 0, "the driver did not copy into the half of the page the title did not store into");

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    check(!header.empty(), "the capture has no capture.json");
    deltaReadInFlight(reader, layout.secondCommands);
    std::vector<std::uint64_t> flagged;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::Memory && (call.flags & AgcDriver::InFlightDeltaFlag) != 0 && !call.values.empty()) flagged.push_back(call.values.front());
    }
    const auto number = [&](const std::string& key) -> std::uint64_t {
        const auto found = header.find("\"" + key + "\":");
        return found == std::string::npos ? 0 : std::stoull(header.substr(found + key.size() + 3));
    };
    const auto& calls = reader.Calls();
    const auto end = std::find_if(calls.begin(), calls.end(), [](const auto& call) { return call.kind == FrameCallKind::End; });
    if (end == calls.end() || number("frames") != 1) throw Inconclusive("the capture stopped before it recorded its end, for example because the copy behind the labels landed before the last frame's call, so the check at the end of the capture did not run");
    if (end->completed > submitted(reader, words).accepted) throw Inconclusive("the labels and the copy behind them completed before the capture recorded its end, so the store did not land after the capture's last call");
    check(std::find(flagged.begin(), flagged.end(), number("stale_delta")) != flagged.end() || number("unverified_deltas") != 0, "the copy behind the labels stored into the page of a delta read while it was in flight, after the capture's last frame, and the capture was kept with no delta stale or unverified: " + header);
    const auto log = std::filesystem::path(directory) / "late store.log";
    auto command = "\"" + tool.string() + "\" \"" + std::string(directory) + "\" > \"" + (std::filesystem::path(directory) / "late store.json").string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    check(std::system(command.c_str()) != 0, "the replay accepted a capture whose delta read in flight was never checked against the stores that landed after its last frame");
    check(!observed, "the frame recorder still observed the driver's stores after the capture ended");
}

constexpr std::uint32_t BusyWrites = 20000;

bool awaits(const std::uint32_t* word) {
    std::array<AgcDriver::AwaitedWait, 64> waits{};
    const auto count = std::min(AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size()), waits.size());
    return std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(word); });
}

enum class WaitKind { Wait32, Wait64, Semaphore, Rewind };

enum class LabelPlacement { None, Behind, AheadRejected, AheadSkipped, ElsewhereRejected };

std::uint32_t* emitAwait(CommandBuffer& buffer, WaitKind kind, std::byte* flag) {
    switch (kind) {
    case WaitKind::Wait32:
        sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, flag, Done, 0xffffffffu, 0);
        return reinterpret_cast<std::uint32_t*>(flag);
    case WaitKind::Wait64:
        sceAgcDcbWaitRegMem(&buffer, 1, 3, 0, 0, flag, Done, ~std::uint64_t{0}, 0);
        return reinterpret_cast<std::uint32_t*>(flag);
    case WaitKind::Semaphore:
        sceAgcDcbMemSemaphore(&buffer, flag, 0, 0, 7);
        return reinterpret_cast<std::uint32_t*>(flag);
    case WaitKind::Rewind:
        return sceAgcDcbRewind(&buffer, 0) + 1;
    }
    throw std::logic_error("unknown wait kind");
}

void passAwait(WaitKind kind, std::uint32_t* word) {
    if (kind == WaitKind::Rewind) release(word);
    else if (kind == WaitKind::Wait32) std::atomic_ref<std::uint32_t>(*word).store(Done, std::memory_order_release);
    else std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(word)).store(Done, std::memory_order_release);
}

Packet awaitingCopy(const Layout& busy, const Layout& layout, const std::function<void(CommandBuffer&)>& wait, bool waitFirst) {
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const end = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer buffer{words, end, words, end, nullptr, nullptr, 0};
    const auto* const label = reinterpret_cast<const std::uint32_t*>(busy.base + BusyLabels);
    if (waitFirst) wait(buffer);
    for (std::uint32_t index = 0; index < BusyWrites; ++index) writeDone(buffer, label);
    if (!waitFirst) wait(buffer);
    sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    return Packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
}

Packet warmingPacket(const Layout& busy) {
    return labelPacket(reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels + CommandBytes), reinterpret_cast<const std::uint32_t*>(busy.base + BusyLabels) + 1);
}

Packet clearingCompute(const Layout& other, const std::byte* flag) {
    auto* const words = reinterpret_cast<std::uint32_t*>(other.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(other.base + BusyLabels);
    CommandBuffer buffer{words, labels, words, labels, nullptr, nullptr, 0};
    const std::uint32_t cleared = 0;
    sceAgcAcbWriteData(&buffer, ComputeMemory, 0, at(flag), &cleared, 1, 0, 1);
    for (std::uint32_t index = 0; index < BusyWrites; ++index) sceAgcAcbWriteData(&buffer, ComputeMemory, 0, at(labels + 1), &Done, 1, 0, 1);
    sceAgcAcbWriteData(&buffer, ComputeMemory, 0, at(labels), &Done, 1, 0, 1);
    return Packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
}

void submitLabel(std::uint32_t* words, const std::uint32_t* label) {
    auto buffer = commands(words);
    writeDone(buffer, label);
    Packet packet{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the submission after the title's store failed");
}

void checkTitleWaitsHeld(const std::filesystem::path& tool, const Packet& awaiting, const std::byte* target, const std::vector<std::byte>& copied, const std::string& options = " --no-release", bool unpassedAtRead = false) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const FrameCaptureReader reader(capture);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    check(!reader.Truncated(), "the capture is truncated");
    check(header.find("stopped") == std::string::npos, "the capture stopped: " + header);
    check(header.find("\"frames\":2") != std::string::npos, "the capture header does not record two frames: " + header);
    const auto submit = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(awaiting.addr); });
    check(submit != calls.end(), "the capture does not hold the submission that waits on the title's flag");
    const auto serial = submit->accepted + 1;
    if (unpassedAtRead) check(jsonCount(jsonCounts(jsonObject(header, "hold_reasons")), "title_wait_unpassed", header) >= 1, "the capture did not count the delta read at the title's next call, while submission " + std::to_string(serial) + " still had to pass its wait on the title's flag and no label in flight stored a value that wait accepts, as title_wait_unpassed: " + header);
    check(std::none_of(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && (call.flags & AgcDriver::InFlightDeltaFlag) != 0 && std::find(call.values.begin() + 1, call.values.end(), serial) != call.values.end(); }), "the capture read a memory delta in flight while submission " + std::to_string(serial) + ", in flight at the read, still depended on the flag the title stores: " + header);
    check(jsonCount(jsonCounts(jsonObject(header, "hold_reasons")), "awaits_title", header) >= 1, "the capture did not count the submission's wait on the title's flag as a hold reason: " + header);
    check(replayed(tool, capture, target, DataBytes, options) == copied, "the replay did not copy the title's store after the submission passed its wait on the title's flag");
}

void checkNeverReadInFlight(const Packet& awaiting) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    const auto submit = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(awaiting.addr); });
    check(submit != calls.end(), "the capture does not hold the submission that waits on the title's semaphore");
    const auto serial = submit->accepted + 1;
    check(std::none_of(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && (call.flags & AgcDriver::InFlightDeltaFlag) != 0 && std::find(call.values.begin() + 1, call.values.end(), serial) != call.values.end(); }), "the capture read a memory delta in flight while submission " + std::to_string(serial) + ", in flight at the read, still waited on the semaphore the title signals: " + header);
    check(jsonCount(jsonCounts(jsonObject(header, "hold_reasons")), "awaits_title", header) >= 1, "the capture did not count the submission's semaphore wait as a hold reason: " + header);
    check(header.find("stopped") == std::string::npos && header.find("\"frames\":2") != std::string::npos, "the capture stopped or did not record both frames, although the title signalled the semaphore once the submission reached it: " + header);
}

void checkAwaitAfterRead(const std::filesystem::path& tool, WaitKind kind, std::uint8_t seed, LabelPlacement placement = LabelPlacement::None) {
    constexpr std::uint32_t ComputeQueue = 0x20;
    const auto layout = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &busy}) std::memset(touched->base, 0, BlockBytes);
    auto* const flag = placement == LabelPlacement::None ? layout.secondTarget : allocate().base;
    std::uint32_t* word = nullptr;
    auto* const marker = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels) + 2;
    const auto awaiting = awaitingCopy(busy, layout, [&](CommandBuffer& buffer) {
        if (kind == WaitKind::Semaphore) writeDone(buffer, marker);
        if (placement == LabelPlacement::AheadRejected) {
            const std::uint32_t cleared = 0;
            sceAgcDcbWriteData(&buffer, GraphicsMemory, 0, at(flag), &cleared, 1, 0, 1);
        }
        if (placement == LabelPlacement::AheadSkipped) {
            sceAgcDcbCondExec(&buffer, reinterpret_cast<const std::uint32_t*>(flag + DataBytes), sceAgcDcbWriteDataGetSize(1) / sizeof(std::uint32_t));
            writeDone(buffer, reinterpret_cast<const std::uint32_t*>(flag));
        }
        word = emitAwait(buffer, kind, flag);
        if (placement == LabelPlacement::Behind) writeDone(buffer, word);
    }, false);
    const auto other = placement == LabelPlacement::ElsewhereRejected ? allocate() : Layout{};
    const auto clearing = placement == LabelPlacement::ElsewhereRejected ? clearingCompute(other, flag) : Packet{};
    const auto otherLabel = [&](std::size_t index) { return std::atomic_ref<std::uint32_t>(reinterpret_cast<std::uint32_t*>(other.base + BusyLabels)[index]).load(std::memory_order_acquire); };
    const auto warming = warmingPacket(busy);
    const auto copied = pattern(seed);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&awaiting) == 0, "the submission that waits on the title's flag failed");
    if (placement == LabelPlacement::ElsewhereRejected) check(sceAgcDriverSubmitAcb(ComputeQueue, &clearing) == 0, "the compute submission that clears the title's flag failed");
    awaitStarted();
    if (awaits(word)) throw Inconclusive("the submission reached its wait on the title's flag before the title's next call; raise BusyWrites");
    if (placement == LabelPlacement::ElsewhereRejected && otherLabel(0) == Done) throw Inconclusive("the compute submission that clears the title's flag finished before the title's next call; raise BusyWrites");
    std::memcpy(layout.source, copied.data(), copied.size());
    if (kind == WaitKind::Semaphore && std::atomic_ref<std::uint32_t>(*marker).load(std::memory_order_acquire) == Done) throw Inconclusive("the submission reached its semaphore before the title's next call; raise BusyWrites");
    bool reached = false;
    std::atomic<bool> calling{false};
    std::jthread signaller;
    if (kind == WaitKind::Semaphore) {
        signaller = std::jthread([&] {
            const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while ((!calling.load(std::memory_order_acquire) || std::atomic_ref<std::uint32_t>(*marker).load(std::memory_order_acquire) != Done) && std::chrono::steady_clock::now() < limit) std::this_thread::yield();
            reached = std::atomic_ref<std::uint32_t>(*marker).load(std::memory_order_acquire) == Done;
            passAwait(kind, word);
        });
    }
    calling.store(true, std::memory_order_release);
    submitLabel(layout.firstCommands, layout.label + 2);
    if (kind == WaitKind::Semaphore) {
        signaller.join();
        check(reached, "the submission did not reach its semaphore within 10 s after the title's next call");
    } else {
        if (placement == LabelPlacement::AheadRejected) waitUntil([&] { return awaits(word); }, "the submission did not reach its wait after it cleared the title's flag");
        if (placement == LabelPlacement::ElsewhereRejected) waitUntil([&] { return otherLabel(1) == Done; }, "the compute submission did not clear the title's flag");
        passAwait(kind, word);
    }
    submitLabel(layout.secondCommands, layout.label + 3);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not copy the title's store after the title stored the flag");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    if (kind == WaitKind::Semaphore) checkNeverReadInFlight(awaiting);
    else checkTitleWaitsHeld(tool, awaiting, layout.firstTarget, copied, placement == LabelPlacement::Behind ? "" : " --no-release", true);
}

void testWaitAfterRead(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait32, 83);
}

void testWait64AfterRead(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait64, 101);
}

void testSemaphoreAfterRead(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Semaphore, 103);
}

void testRewindAfterRead(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Rewind, 107);
}

void testWaitLabelledAfter(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait32, 109, LabelPlacement::Behind);
}

void testWaitLabelledBefore(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait32, 113, LabelPlacement::AheadRejected);
}

void testWaitLabelledSkipped(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait32, 127, LabelPlacement::AheadSkipped);
}

void testWaitLabelledElsewhere(const std::filesystem::path& tool) {
    checkAwaitAfterRead(tool, WaitKind::Wait32, 131, LabelPlacement::ElsewhereRejected);
}

void checkWaitLabelledRewind(const std::filesystem::path& tool, bool chained) {
    const auto layout = allocate();
    const auto busy = allocate();
    const auto gate = allocate();
    for (const auto* touched : {&layout, &busy, &gate}) std::memset(touched->base, 0, BlockBytes);
    auto* const flag = gate.label;
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer buffer{words, labels, words, labels, nullptr, nullptr, 0};
    const std::uint32_t cleared = 0;
    if (chained) writeDone(buffer, flag);
    auto* const first = chained ? sceAgcDcbRewind(&buffer, 0) + 1 : nullptr;
    sceAgcDcbWriteData(&buffer, GraphicsMemory, 0, at(flag), &cleared, 1, 0, 1);
    release(sceAgcDcbRewind(&buffer, 0) + 1);
    writeDone(buffer, labels + 2);
    for (std::uint32_t index = 0; index < BusyWrites; ++index) writeDone(buffer, labels);
    sceAgcDcbWaitRegMem(&buffer, 0, 3, 0, 0, flag, Done, 0xffffffffu, 0);
    sceAgcDcbDmaData(&buffer, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(layout.source), DataBytes, 0, 1, 0);
    const Packet awaiting{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    const auto labelling = labelPacket(gate.firstCommands, flag);
    const auto warming = warmingPacket(busy);
    const auto copied = chained ? pattern(139) : pattern(137);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&labelling) == 0, "the submission that labels the title's flag failed");
    AgcDriverWaitIdle_nid_postfix();
    std::uint64_t labeller = 0;
    std::uint64_t completed = 0;
    AgcDriverSerialProgress_nid_postfix(&labeller, &completed);
    AgcDriver::DriverDetail::DriverTestAccess::KeepLabelRecord(labeller, flag, Done);
    check(sceAgcDriverSubmitDcb(&awaiting) == 0, "the submission that waits on the title's flag in its REWIND tail failed");
    if (chained) {
        waitUntil([&] { return awaits(first); }, "the submission did not park on its first REWIND after it labelled the title's flag");
        release(first);
    }
    waitUntil([&] { return std::atomic_ref<std::uint32_t>(labels[2]).load(std::memory_order_acquire) == Done; }, "the submission did not start its REWIND tail");
    if (awaits(flag)) throw Inconclusive("the REWIND tail reached its wait on the title's flag before the title's next call; raise BusyWrites");
    std::memcpy(layout.source, copied.data(), copied.size());
    submitLabel(layout.firstCommands, layout.label + 2);
    AgcDriver::DriverDetail::DriverTestAccess::ForgetLabelRecord(labeller);
    passAwait(WaitKind::Wait32, flag);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not copy the title's store after the title stored the flag");
    submitLabel(layout.secondCommands, layout.label + 3);
    AgcDriverWaitIdle_nid_postfix();
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    check(!reader.Truncated(), "the capture is truncated");
    check(header.find("stopped") == std::string::npos, "the capture stopped: " + header);
    check(header.find("\"frames\":2") != std::string::npos, "the capture header does not record two frames: " + header);
    const std::string rejecting = chained ? "the label of its first REWIND tail stored a value that wait rejects, and only the submission's label before its first REWIND and submission " : "the submission's label before its REWIND stored a value that wait rejects, and only submission ";
    check(jsonCount(jsonCounts(jsonObject(header, "hold_reasons")), "title_wait_unpassed", header) >= 1, std::string("the capture did not count the delta read at the title's next call as title_wait_unpassed, while the wait in the ") + (chained ? "second " : "") + "REWIND tail of submission " + std::to_string(submitted(reader, words).accepted + 1) + " still had to pass on the title's flag: " + rejecting + std::to_string(labeller) + ", whose label record the test kept as a deferred completion keeps it, stored the value it accepts: " + header);
    checkTitleWaitsHeld(tool, awaiting, layout.firstTarget, copied);
}

void testWaitLabelledRewind(const std::filesystem::path& tool) {
    checkWaitLabelledRewind(tool, false);
}

void testWaitLabelledRewindChain(const std::filesystem::path& tool) {
    checkWaitLabelledRewind(tool, true);
}

void testWaitAfterLastFrame(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto running = busyPacket(busy);
    auto gated = commands(layout.secondCommands);
    auto* const flag = emitAwait(gated, WaitKind::Wait32, layout.secondTarget);
    sceAgcDcbDmaData(&gated, 0, DmaMemory, 0, at(layout.firstTarget), DmaMemory, 0, at(written.source), DataBytes, 0, 1, 0);
    const Packet waiting{layout.secondCommands, static_cast<std::uint32_t>(gated.cursor_up - layout.secondCommands), 0, {}};
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the submission warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    const auto kept = pattern(61);
    std::memcpy(written.source + 16 * DataBytes, kept.data(), kept.size());
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(59);
    std::memcpy(written.source, stored.data(), stored.size());
    check(sceAgcDriverSubmitDcb(&waiting) == 0, "the submission that waits on the title's flag failed");
    flip();
    passAwait(WaitKind::Wait32, flag);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, stored), "the driver did not copy the title's store after the title stored the flag");

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const FrameCaptureReader reader(capture);
    const auto& header = reader.Header();
    deltaReadInFlight(reader, layout.secondCommands);
    check(!reader.Truncated() && header.find("stopped") == std::string::npos && header.find("\"frames\":1,") != std::string::npos, "the capture of one frame did not end normally: " + header);
    check(header.find("\"stale_delta\":0,\"unverified_deltas\":0,") != std::string::npos, "the capture left the delta read while the busy submission ran unverified after that submission completed, because a later submission waited on a flag the title stored only after the capture: " + header);
    check(replayed(tool, capture, written.source, DataBytes) == stored, "the replay refused the capture or did not restore the page the title stored while the busy submission ran");
}

void testLastFrameEndedOnNewThread(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto written = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &written, &busy}) std::memset(touched->base, 0, BlockBytes);
    const auto running = busyPacket(busy);
    const auto closing = labelPacket(layout.secondCommands, layout.label + 1);
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&closing) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    flip();
    AgcDriverWaitIdle_nid_postfix();
    const auto token = AgcDriverReserveEopFlip_nid_postfix(AgcDriver::FlipInfo{Handle, -1, 1, 0});
    check(token != 0, "the EOP flip that ends the frame from the REWIND tail was not reserved");
    auto* const words = reinterpret_cast<std::uint32_t*>(busy.base);
    auto* const labels = reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels);
    CommandBuffer buffer{words, labels, words, labels, nullptr, nullptr, 0};
    release(sceAgcDcbRewind(&buffer, 0) + 1);
    for (std::size_t index = 0; index < BusyPackets; ++index) writeDone(buffer, labels + index);
    const auto flipped = at(layout.label + 2);
    const std::array<std::uint32_t, 8> eop{0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(flipped), static_cast<std::uint32_t>(flipped >> 32u), Done, 0u, token};
    std::memcpy(buffer.cursor_up, eop.data(), sizeof(eop));
    buffer.cursor_up += eop.size();
    const Packet rewound{words, static_cast<std::uint32_t>(buffer.cursor_up - words), 0, {}};
    std::atomic<bool> go{false};
    std::exception_ptr failure;
    std::jthread ending([&](std::stop_token stop) {
        while (!go.load(std::memory_order_acquire)) {
            if (stop.stop_requested()) return;
            std::this_thread::yield();
        }
        try {
            check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission of the thread that made no recorded call failed");
        } catch (...) {
            failure = std::current_exception();
        }
    });
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto tailFlips = driver.RewindTailFlips();
    check(sceAgcDriverSubmitDcb(&rewound) == 0, "the submission whose released REWIND tail ends the frame with an EOP flip failed");
    const auto stored = pattern(157);
    std::memcpy(written.source, stored.data(), stored.size());
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (driver.RewindTailFlips() == tailFlips) {
        check(std::chrono::steady_clock::now() < limit, "the driver did not start the released REWIND tail, whose EOP flip ends the captured frame, within 10 s");
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);
    ending.join();
    if (failure) std::rethrow_exception(failure);
    AgcDriverWaitIdle_nid_postfix();
    check(std::atomic_ref<std::uint32_t>(layout.label[2]).load(std::memory_order_acquire) == Done, "the driver did not run the EOP flip at the end of the REWIND tail");

    const auto capture = checkCapture(0);
    const FrameCaptureReader reader(capture);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    const auto& submit = submitted(reader, words);
    const auto start = std::find_if(calls.begin(), calls.end(), [](const auto& call) { return call.kind == FrameCallKind::FrameStart; });
    const auto end = std::find_if(start, calls.end(), [](const auto& call) { return call.kind == FrameCallKind::End; });
    check(start != calls.end() && end != calls.end(), "the capture has no frame start or no end: " + header);
    const auto& last = *std::prev(end);
    check(last.kind == FrameCallKind::Memory && last.ordinal == submit.ordinal + 1 && last.thread == end->thread && end->thread != submit.thread, "the capture did not end with a delta read right after the submission of the REWIND tail, on the thread other than the title's that reached the frame boundary of the tail's flip, so finish() did not run on a thread without a recorded call: " + header);
    check(std::all_of(start, std::prev(end), [&](const auto& call) { return call.thread == submit.thread; }), "a call before the capture's last delta was recorded on a thread other than the title's, so the thread that reached the frame boundary of the REWIND tail's flip had made a recorded call: " + header);
    const auto masks = jsonCounts(jsonObject(header, "hold_masks"));
    if (masks.empty()) throw Inconclusive("the REWIND tail's labels finished before the thread without a recorded call reached the frame boundary of its flip, so the capture's last delta was read with nothing in flight; raise BusyPackets");
    check((last.flags & AgcDriver::InFlightDeltaFlag) == 0, "the capture's last delta was read in flight in finish() on a thread that had made no recorded call; finish() must count its own thread among the calling threads so that delta is held, because the replay refuses a delta read in flight after a second thread made calls: " + header);
    check(masks == std::map<std::string, std::uint64_t>{{"threads", 1}} && jsonCount(jsonCounts(jsonObject(header, "hold_reasons")), "threads", header) == 1, "the capture's last delta was not held for its second calling thread alone, or another delta was read with work in flight, so the capture does not show that counting finish()'s own thread is what holds it: " + header);
    const auto group = last.values.at(0);
    check(std::any_of(reader.Chunks().begin(), reader.Chunks().end(), [&](const auto& chunk) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.group != group || chunk.address > at(written.source) || chunk.address + chunk.bytes < at(written.source) + DataBytes) return false;
        const auto bytes = reader.Read(chunk);
        return std::equal(stored.begin(), stored.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at(written.source) - chunk.address));
    }), "the capture's last delta does not hold the page the title stored while the REWIND tail ran: " + header);
    check(replayed(tool, capture, written.source, DataBytes) == stored, "the replay refused the capture or did not restore the page the title stored before a thread without a recorded call ended the frame");
}

void testWaitBeforeRead(const std::filesystem::path& tool) {
    const auto layout = allocate();
    const auto busy = allocate();
    for (const auto* touched : {&layout, &busy}) std::memset(touched->base, 0, BlockBytes);
    auto* const flag = reinterpret_cast<std::uint32_t*>(layout.secondTarget);
    const auto copied = pattern(89);
    const auto awaiting = awaitingCopy(busy, layout, [&](CommandBuffer& buffer) { emitAwait(buffer, WaitKind::Wait32, layout.secondTarget); }, true);
    const auto warming = warmingPacket(busy);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&awaiting) == 0, "the submission that waits on the title's flag failed");
    waitUntil([&] { return awaits(flag); }, "the submission never waited on the title's flag");
    std::memcpy(layout.source, copied.data(), copied.size());
    std::atomic_ref<std::uint32_t>(*flag).store(Done, std::memory_order_release);
    waitUntil([&] { return !awaits(flag); }, "the submission did not pass its wait after the title stored the flag");
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    AgcDriverSerialProgress_nid_postfix(&accepted, &completed);
    if (completed >= accepted) throw Inconclusive("the submission finished before the title's next call; raise BusyWrites");
    submitLabel(layout.firstCommands, layout.label + 2);
    AgcDriverWaitIdle_nid_postfix();
    check(equal(layout.firstTarget, copied), "the driver did not copy the title's store after the title stored the flag");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    checkTitleWaitsHeld(tool, awaiting, layout.firstTarget, copied);
}

void checkUnwatchedInFlight(const std::filesystem::path& tool, bool restored) {
    const auto layout = allocate();
    const auto other = allocate();
    const auto busy = allocate();
    const auto bulk = allocate();
    for (const auto* touched : {&layout, &other, &busy, &bulk}) std::memset(touched->base, 0, BlockBytes);
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base);
    AgcDriver::GuestMemory::Unwatch(at(bulk.base), 4096);
    check(!AgcDriver::GuestMemory::Watched(at(bulk.base), 4096), "the test buffer did not lose write-watch coverage");
    auto running = busyPacket(busy, restored ? word : nullptr);
    auto warm = commands(layout.firstCommands);
    writeDone(warm, layout.label);
    Packet warming{layout.firstCommands, static_cast<std::uint32_t>(warm.cursor_up - layout.firstCommands), 0, {}};
    auto behind = commands(layout.secondCommands);
    writeDone(behind, layout.label + 1);
    Packet closing{layout.secondCommands, static_cast<std::uint32_t>(behind.cursor_up - layout.secondCommands), 0, {}};
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    for (int pass = 0; pass < 2; ++pass) {
        check(sceAgcDriverSubmitDcb(&warming) == 0, "the warming submission failed");
        AgcDriverWaitIdle_nid_postfix();
    }
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    if (restored) {
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::atomic_ref<std::uint32_t>(*word).load(std::memory_order_acquire) != Done) {
            check(std::chrono::steady_clock::now() < limit, "the busy submission did not store into the unwatched block within 10 s");
            std::this_thread::yield();
        }
        std::atomic_ref<std::uint32_t>(*word).store(0, std::memory_order_release);
    }
    const auto stored = pattern(149);
    std::memcpy(other.source, stored.data(), stored.size());
    if (restored) {
        std::uint64_t accepted = 0;
        std::uint64_t completed = 0;
        AgcDriverSerialProgress_nid_postfix(&accepted, &completed);
        const auto halfway = std::atomic_ref<std::uint32_t>(reinterpret_cast<std::uint32_t*>(busy.base + BusyLabels)[BusyPackets / 2]).load(std::memory_order_acquire);
        if (completed >= accepted || halfway == Done) throw Inconclusive("the busy submission passed half of its labels before the title's next call; raise BusyPackets");
    }
    check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission behind the busy one failed");
    AgcDriverWaitIdle_nid_postfix();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(layout.label[1] == Done && reinterpret_cast<const std::uint32_t*>(busy.base + BusyLabels)[BusyPackets - 1] == Done && *word == 0, "the driver did not run the busy submission and the one behind it");

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    const auto submit = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(layout.secondCommands); });
    check(submit != calls.end(), "the capture lost the submission behind the busy one: " + header);
    check(!restored || submit->accepted != submit->completed, "the call behind the busy submission was recorded with nothing in flight, although the busy submission was still running before the call: " + header);
    if (submit->accepted == submit->completed) throw Inconclusive("the busy submission finished before the title's next call, so no delta was read while it ran");
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == submit->ordinal; });
    check(delta != calls.end(), "the capture recorded no memory delta for the page the title wrote while the busy submission ran: " + header);
    const auto reasons = jsonCounts(jsonObject(header, "hold_reasons"));
    if (!restored) check(jsonCount(reasons, "unwatched", header) == 0, "the delta read while the busy submission ran was held for an unwatched block that the title left unchanged and the driver never stored into: " + header);
    const auto entry = header.find("{\"address\":" + std::to_string(at(bulk.base)) + ",\"bytes\":" + std::to_string(AgcDriver::FrameCaptureWriter::BlockBytes) + ",");
    check(entry != std::string::npos, "the capture header does not list the unwatched block: " + header);
    const auto block = jsonCounts(header.substr(entry + 1, header.find('}', entry) - entry - 1));
    if (!restored) {
        check((delta->flags & AgcDriver::InFlightDeltaFlag) != 0, "the delta read while the busy submission ran was not read in flight: " + header);
        check(jsonCount(block, "changed", header) == 0 && jsonCount(block, "driver_stored", header) == 0, "the unwatched block's counters show a change or a driver store that never happened: " + header);
        check(replayed(tool, checkCapture(1), other.source, DataBytes) == stored, "the replay did not restore the page the title wrote while the busy submission ran");
        return;
    }
    check(jsonCount(reasons, "unwatched", header) >= 1 && (delta->flags & AgcDriver::InFlightDeltaFlag) == 0, "the delta read after the busy submission stored into the unwatched block was not held for it, although the title had written the recorded bytes back: " + header);
    check(jsonCount(block, "driver_stored", header) >= 1, "the unwatched block's counters show no driver store: " + header);
    check(replayed(tool, checkCapture(0), word, 4) == std::vector<std::byte>(4), "the replay kept the busy submission's store into the unwatched block over the title's later write of the recorded bytes");
}

void testUnwatchedRewatchedInFlight(const std::filesystem::path& tool) {
#ifndef _WIN32
    static_cast<void>(tool);
    throw Inconclusive("Rewatch notes no stores on this host, so a block cannot return to the write watch");
#else
    static_cast<void>(tool);
    const auto layout = allocate();
    const auto busy = allocate();
    const auto bulk = allocate();
    for (const auto* touched : {&layout, &busy, &bulk}) std::memset(touched->base, 0, BlockBytes);
    constexpr std::uint32_t Recorded = 0x41414141u;
    constexpr std::uint32_t Rewritten = 0x42424242u;
    auto* const word = reinterpret_cast<std::uint32_t*>(bulk.base);
    std::atomic_ref<std::uint32_t>(*word).store(Recorded, std::memory_order_release);
    AgcDriver::GuestMemory::Unwatch(at(bulk.base), BlockBytes);
    check(!AgcDriver::GuestMemory::Watched(at(bulk.base), BlockBytes), "the test block did not lose write-watch coverage");
    const auto running = busyPacket(busy);
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    auto behind = commands(layout.secondCommands);
    writeDone(behind, layout.label + 1);
    const Packet closing{layout.secondCommands, static_cast<std::uint32_t>(behind.cursor_up - layout.secondCommands), 0, {}};
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(sceAgcDriverSubmitDcb(&warming) == 0, "the warming submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const std::pair<std::uint64_t, std::uint64_t> block{at(bulk.base), at(bulk.base) + BlockBytes};
    AgcDriver::GuestMemory::Rewatch(std::span(&block, 1));
    check(AgcDriver::GuestMemory::Watched(at(bulk.base), BlockBytes), "the test block did not return to the write watch");
    check(sceAgcDriverSubmitDcb(&warming) == 0, "the warming submission failed");
    AgcDriverWaitIdle_nid_postfix();
    std::atomic_ref<std::uint32_t>(*word).store(Rewritten, std::memory_order_release);
    check(sceAgcDriverSubmitDcb(&warming) == 0, "the warming submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    std::atomic_ref<std::uint32_t>(*word).store(Recorded, std::memory_order_release);
    AgcDriver::GuestMemory::Unwatch(at(bulk.base), BlockBytes);
    check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission behind the busy one failed");
    AgcDriverWaitIdle_nid_postfix();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(std::atomic_ref<std::uint32_t>(*word).load(std::memory_order_acquire) == Recorded, "the title's last store into the test block was lost");

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& submit = submitted(reader, layout.secondCommands);
    if (submit.accepted == submit.completed) throw Inconclusive("the busy submission finished before the title's next call, so no delta was read while it ran");
    const auto& calls = reader.Calls();
    const auto delta = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Memory && call.ordinal + 1 == submit.ordinal; });
    if (delta != calls.end() && (delta->flags & AgcDriver::InFlightDeltaFlag) == 0) throw Inconclusive("the delta before the call behind the busy submission was held, so it read the unwatched block again after the settle");
    bool listed = false;
    if (delta != calls.end()) {
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind != FrameMemoryKind::Delta || chunk.group != delta->values.at(0) || chunk.address > at(word) || chunk.address + chunk.bytes < at(word) + sizeof(std::uint32_t)) continue;
            const auto bytes = reader.Read(chunk);
            std::uint32_t value = 0;
            std::memcpy(&value, bytes.data() + (at(word) - chunk.address), sizeof(value));
            listed = listed || value == Recorded;
        }
    }
    check(listed, "the delta read while the busy submission ran left out the unwatched block whose bytes the title wrote back after a delta listed it while it was watched, so a replay keeps the bytes it wrote in between: " + reader.Header());
#endif
}

void testUnwatchedLeftInFlight(const std::filesystem::path& tool) {
    checkUnwatchedInFlight(tool, false);
}

void testUnwatchedRestoredInFlight(const std::filesystem::path& tool) {
    checkUnwatchedInFlight(tool, true);
}

void testHostPageRestoredInFlight(const std::filesystem::path&) {
    constexpr std::size_t PageBytes = 4096;
    constexpr std::size_t Offset = 2048;
    constexpr std::size_t Bytes = 1024;
    const auto layout = allocate();
    std::memset(layout.base, 0, BlockBytes);
#ifdef _WIN32
    auto* const page = static_cast<std::byte*>(VirtualAlloc(nullptr, PageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    void* const mapped = mmap(nullptr, PageBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    auto* const page = mapped != MAP_FAILED ? static_cast<std::byte*>(mapped) : nullptr;
#endif
    check(page != nullptr, "cannot allocate the host page");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(page, PageBytes, true, true, true);
    }
    auto* const shader = reinterpret_cast<Shader*>(page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto recorded = pattern(163);
    auto registered = pattern(167);
    recorded.resize(Bytes);
    registered.resize(Bytes);
    std::memcpy(page + Offset, recorded.data(), Bytes);
    auto gated = commands(layout.firstCommands);
    auto* const flag = emitAwait(gated, WaitKind::Wait32, layout.secondTarget);
    writeDone(gated, layout.label);
    const Packet waiting{layout.firstCommands, static_cast<std::uint32_t>(gated.cursor_up - layout.firstCommands), 0, {}};
    auto* const recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "the frame recorder is disabled");
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    AgcDriverWaitIdle_nid_postfix();
    check(sceAgcDriverSubmitDcb(&waiting) == 0, "the submission that waits on the title's flag failed");
    waitUntil([&] { return awaits(flag); }, "the submission never waited on the title's flag");
    std::memcpy(page + Offset, registered.data(), Bytes);
    recorder->RegisterShader(shader);
    std::memcpy(page + Offset, recorded.data(), Bytes);
    submitLabel(layout.secondCommands, layout.label + 1);
    passAwait(WaitKind::Wait32, flag);
    AgcDriverWaitIdle_nid_postfix();
    check(layout.label[0] == Done && layout.label[1] == Done, "the driver did not run the submission that waited on the title's flag and the one behind it");
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    const auto& chunks = reader.Chunks();
    check(!reader.Truncated() && header.find("stopped") == std::string::npos && header.find("\"frames\":2") != std::string::npos, "the capture did not end normally: " + header);
    check(std::any_of(chunks.begin(), chunks.end(), [&](const auto& chunk) { return chunk.kind == FrameMemoryKind::Range && chunk.rangeKind == AgcDriver::FrameRangeKind::Host && chunk.address <= at(page) && at(page) + PageBytes <= chunk.address + chunk.bytes; }), "the capture header does not list a Host range for the title's host page: " + header);
    const auto submit = std::find_if(calls.begin(), calls.end(), [&](const auto& call) { return call.kind == FrameCallKind::Submit && call.address == at(layout.secondCommands); });
    check(submit != calls.end(), "the capture lost the submission after the title wrote its host page back: " + header);
    check(submit->accepted != submit->completed, "the submission after the title wrote its host page back was recorded with nothing in flight, although the submission before it waited on the title's flag: " + header);
    std::vector<std::byte> captured(Bytes);
    const auto apply = [&](const AgcDriver::FrameMemoryChunk& chunk) {
        const auto begin = std::max(at(page + Offset), chunk.address);
        const auto end = std::min<std::uint64_t>(at(page + Offset) + Bytes, chunk.address + chunk.bytes);
        if (begin >= end) return;
        const auto data = reader.Read(chunk);
        std::memcpy(captured.data() + (begin - at(page + Offset)), data.data() + (begin - chunk.address), static_cast<std::size_t>(end - begin));
    };
    for (const auto& chunk : chunks) {
        if (chunk.kind == FrameMemoryKind::Snapshot) apply(chunk);
    }
    bool shaderDelta = false;
    for (auto call = calls.begin(); call != submit; ++call) {
        if (call->kind == FrameCallKind::RegisterShader && call->address == at(shader)) shaderDelta = captured == registered;
        if (call->kind != FrameCallKind::Memory) continue;
        for (const auto& chunk : chunks) {
            if (chunk.kind == FrameMemoryKind::Delta && chunk.group == call->values.at(0)) apply(chunk);
        }
    }
    check(shaderDelta, "the shader delta of the title's registration did not record the bytes the title wrote into its host page: " + header);
    check(captured == recorded, "the capture holds the bytes a shader delta wrote into a host page at the next full delta taken with a submission in flight, although the title had written back the bytes that the full delta before the shader delta recorded");
}

std::byte* allocateCpuBlock(std::size_t bytes = BlockBytes) {
    auto* const block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    check(block != nullptr, "cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, 0x04, bytes);
#ifdef __linux__
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation mutation;
    mutation.Add(block, bytes, true, true, false);
    return block;
}

std::string groupText(const std::vector<std::uint64_t>& groups) {
    if (groups.empty()) return "no memory delta";
    std::string text = groups.size() == 1 ? "memory delta group" : "memory delta groups";
    for (const auto group : groups) text += " " + std::to_string(group);
    return text;
}

std::vector<std::byte> pageInGroup(const FrameCaptureReader& reader, std::uint64_t group, const std::byte* page) {
    std::vector<std::byte> bytes(DataBytes);
    std::uint64_t covered = 0;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.group != group) continue;
        const auto begin = std::max(at(page), chunk.address);
        const auto end = std::min<std::uint64_t>(at(page) + DataBytes, chunk.address + chunk.bytes);
        if (begin >= end) continue;
        const auto data = reader.Read(chunk);
        std::memcpy(bytes.data() + (begin - at(page)), data.data() + (begin - chunk.address), static_cast<std::size_t>(end - begin));
        covered += end - begin;
    }
    if (covered != DataBytes) bytes.clear();
    return bytes;
}

void testShaderDeltaReadInFlight(const std::filesystem::path&) {
    constexpr std::size_t ShaderBlockBytes = AgcDriver::FrameCaptureWriter::BlockBytes;
    const auto layout = allocate();
    const auto busy = allocate();
    auto* const program = allocateCpuBlock();
    std::memset(layout.base, 0, BlockBytes);
    std::memset(busy.base, 0, BlockBytes);
    std::memset(program, 0, BlockBytes);
    auto* const block = program + 2 * ShaderBlockBytes;
    auto* const shader = reinterpret_cast<Shader*>(block);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* const inside = block + 3 * DataBytes;
    auto* const beside = program + 8 * ShaderBlockBytes;
    const auto running = busyPacket(busy);
    const auto warming = labelPacket(layout.firstCommands, layout.label);
    const auto closing = labelPacket(layout.secondCommands, layout.label + 1);
    auto* const recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "the frame recorder is disabled");
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    check(sceAgcDriverSubmitDcb(&running) == 0 && sceAgcDriverSubmitDcb(&closing) == 0, "the submissions warming the imports failed");
    AgcDriverWaitIdle_nid_postfix();

    flip();
    AgcDriverWaitIdle_nid_postfix();
    settleDeltas(warming);
    check(sceAgcDriverSubmitDcb(&running) == 0, "the busy submission failed");
    awaitStarted();
    const auto stored = pattern(71);
    const auto storedBeside = pattern(73);
    std::memcpy(inside, stored.data(), stored.size());
    std::memcpy(beside, storedBeside.data(), storedBeside.size());
    recorder->RegisterShader(shader);
    recorder->RegisterShader(shader);
    check(sceAgcDriverSubmitDcb(&closing) == 0, "the submission behind the busy one failed");
    AgcDriverWaitIdle_nid_postfix();
    flip();
    flip();
    AgcDriverWaitIdle_nid_postfix();

    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const FrameCaptureReader reader(directory);
    const auto& header = reader.Header();
    const auto& calls = reader.Calls();
    check(!reader.Truncated() && header.find("stopped") == std::string::npos && header.find("\"frames\":2") != std::string::npos, "the capture of two frames did not end normally: " + header);
    const auto registersShader = [&](const AgcDriver::FrameCall& call) { return call.kind == FrameCallKind::RegisterShader && call.address == at(shader); };
    const auto first = std::find_if(calls.begin(), calls.end(), registersShader);
    check(first != calls.end(), "the capture lost the title's first registration of the shader: " + header);
    const auto second = std::find_if(std::next(first), calls.end(), registersShader);
    check(second != calls.end(), "the capture lost the title's second registration of the shader: " + header);
    if (first->accepted == first->completed) throw Inconclusive("the busy submission finished before the title registered the shader, so no shader delta was read while it ran");
    check(first != calls.begin() && std::prev(first)->kind == FrameCallKind::Memory && std::prev(first)->ordinal + 1 == first->ordinal, "the capture recorded no memory delta right before the first registration of the shader, although the title stored into the shader's block after the last delta, so a replay would register the shader over bytes the title had already replaced: " + header);
    const auto& shaderDelta = *std::prev(first);
    check((shaderDelta.flags & AgcDriver::InFlightDeltaFlag) != 0, "the shader delta of the registration made while the busy submission ran was held, not read in flight, although nothing in flight could store into the shader's blocks, so every registration made while work runs would wait for that work: " + header);
    const auto busySerial = submitted(reader, reinterpret_cast<const std::uint32_t*>(busy.base)).accepted + 1;
    check(std::find(shaderDelta.values.begin() + 1, shaderDelta.values.end(), busySerial) != shaderDelta.values.end(), "the shader delta read while the busy submission ran does not list that submission as started, so a replay cannot wait for the work the capture saw running at the read: " + header);
    const auto between = std::find_if(std::next(first), second, [](const AgcDriver::FrameCall& call) { return call.kind == FrameCallKind::Memory; });
    if (between != second) {
        const auto group = between->values.at(0);
        std::uint64_t held = 0;
        for (const auto& chunk : reader.Chunks()) held += chunk.kind == FrameMemoryKind::Delta && chunk.group == group ? chunk.bytes : 0;
        const auto again = deltaGroupsHolding(reader, inside);
        const bool relisted = std::find(again.begin(), again.end(), group) != again.end();
        throw std::runtime_error("memory delta group " + std::to_string(group) + " (" + std::to_string(held) + " bytes" + (relisted ? ", among them the page the title stored before the first registration" : "") + ") was recorded between the two registrations of the shader although the title stored nothing into its blocks between them, so the second shader delta taken with work in flight read again a page the first one had read, or recorded a read with nothing to read: " + header);
    }
    if (second->accepted == second->completed) throw Inconclusive("the busy submission finished between the two registrations of the shader, so the second one's shader delta was not taken with work in flight");
    const auto& closingSubmit = submitted(reader, layout.secondCommands);
    const auto full = std::next(second);
    check(full != calls.end() && full->kind == FrameCallKind::Memory && full->ordinal + 1 == closingSubmit.ordinal, "the call after the second registration of the shader is not the memory delta of the submission behind the busy one, although the title stored beside the shader's blocks before the registrations and no delta had read that page since: " + header);
    const auto shaderGroup = shaderDelta.values.at(0);
    const auto fullGroup = full->values.at(0);
    const auto insideGroups = deltaGroupsHolding(reader, inside);
    check(insideGroups == std::vector<std::uint64_t>{shaderGroup, fullGroup}, "the page the title stored into the shader's 64 KiB block before registering it is in " + groupText(insideGroups) + ", not exactly in the shader delta read in flight (group " + std::to_string(shaderGroup) + "), which the registration reads, and once more in the next full delta (group " + std::to_string(fullGroup) + "), which lists it again because a shader delta leaves its ranges' marks where they were: " + header);
    const auto besideGroups = deltaGroupsHolding(reader, beside);
    check(besideGroups == std::vector<std::uint64_t>{fullGroup}, "the page the title stored beside the shader's blocks before registering it is in " + groupText(besideGroups) + ", not only in the next full delta (group " + std::to_string(fullGroup) + "), so the shader delta read in flight reached past the shader's blocks or moved its range's mark past a store it did not read: " + header);
    check(pageInGroup(reader, shaderGroup, inside) == stored && pageInGroup(reader, fullGroup, inside) == stored, "the shader delta read in flight or the next full delta does not hold the bytes the title stored into the shader's block before registering it, so a replay restores other bytes than the title wrote: " + header);
    check(header.find("\"stale_delta\":0,") != std::string::npos && header.find("\"unverified_deltas\":0,") != std::string::npos, "the capture whose shader delta was read in flight ended with a stale or unverified delta although no submission stored into the shader's blocks, so the replay would refuse it: " + header);
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

std::mutex carriedStoresMutex;
std::vector<std::pair<std::uint64_t, std::uint64_t>> carriedStores;

void observeCarriedStore(std::uint64_t begin, std::uint64_t, std::uint64_t serial) {
    std::lock_guard lock(carriedStoresMutex);
    carriedStores.emplace_back(begin, serial);
}

void testCarriedLabelKeepsSerial(const std::filesystem::path&) {
    const auto layout = allocate();
    auto* const gate = layout.label + 1;
    auto* const carried = layout.label + 2;
    auto* const later = layout.label + 3;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    AgcDriverSerialProgress_nid_postfix(&accepted, &completed);
    const auto queued = AgcDriver::DriverDetail::queuedLabels.load();
    const auto skipped = AgcDriver::DriverDetail::submissionCosts(0).endSkipped.load();
    AgcDriver::GuestMemory::SetDriverStoreObserver(&observeCarriedStore);
    auto waiting = commands(layout.firstCommands);
    sceAgcDcbWaitRegMem(&waiting, 0, 3, 0, 0, gate, Done, 0xffffffffu, 0);
    writeDone(waiting, carried);
    Packet first{layout.firstCommands, static_cast<std::uint32_t>(waiting.cursor_up - layout.firstCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&first) == 0, "graphics submission failed");
    waitUntil([&] {
        std::array<AgcDriver::AwaitedWait, 8> waits{};
        const auto count = AgcDriverAwaitedWaits_nid_postfix(waits.data(), waits.size());
        return std::any_of(waits.begin(), waits.begin() + static_cast<std::ptrdiff_t>(count), [&](const auto& wait) { return wait.address == at(gate); });
    }, "the graphics wait never reached the awaited-wait registry");
    auto next = commands(layout.secondCommands);
    writeDone(next, later);
    Packet second{layout.secondCommands, static_cast<std::uint32_t>(next.cursor_up - layout.secondCommands), 0, {}};
    check(sceAgcDriverSubmitDcb(&second) == 0, "graphics submission failed");
    std::atomic_ref<std::uint32_t>(*gate).store(Done, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::SetDriverStoreObserver(nullptr);
    check(std::atomic_ref<std::uint32_t>(*carried).load() == Done && std::atomic_ref<std::uint32_t>(*later).load() == Done, "the labels of the two submissions did not land");
    check(AgcDriver::DriverDetail::queuedLabels.load() - queued == 2, "the worker did not queue the labels of the two submissions");
    check(AgcDriver::DriverDetail::submissionCosts(0).endSkipped.load() > skipped, "the submission that waited stored its queued label at its end although the next submission was queued");
    std::lock_guard lock(carriedStoresMutex);
    const auto requireSerial = [&](const std::uint32_t* label, std::uint64_t serial, const std::string& what) {
        bool stored = false;
        for (const auto& [begin, reported] : carriedStores) {
            if (begin != at(label)) continue;
            stored = true;
            check(reported == serial, what + " was reported under serial " + std::to_string(reported) + ", not under submission " + std::to_string(serial) + " that queued it");
        }
        check(stored, what + " made no observed driver store");
    };
    requireSerial(carried, accepted + 1, "the label that the waiting submission queued and the next submission stored");
    requireSerial(later, accepted + 2, "the label of the next submission");
}

}

int main(int argc, char** argv) {
    const std::string mode = argc == 3 ? argv[2] : "";
    const std::map<std::string, void (*)(const std::filesystem::path&)> rewound{
        {"rewind_store", testStoreAfterGatedCopy}, {"store_idle", testStoreAfterGatedCopyIdle}, {"store_idle_rewind", testStoreAfterRewoundCopyIdle}, {"store_idle_unwatched", testUnwatchedStoreBeforeReservation}, {"store_second_wait", testStoreBeforeSecondWaitOnGate}, {"rewind_copy", testCopyInFlightAtDelta}, {"rewind_compute", testComputeWaitBehindRewind}, {"rewind_release", testReleaseDuringDelta}, {"rewind_fill", testFilledRewindTail}, {"rewind_split", testRewindFilledAcrossChunks}, {"wait_label", testLabelFromLaterSubmission}, {"gpu_store", testGpuStoreBehindRewind}, {"gpu_fill", testFillRewrittenByTitle}, {"gpu_restore_cached", testGpuFillRestoredToCachedValue}, {"unwatched_start", testInitiallyUnwatchedTitleStores}, {"unwatched_late", testLaterUnwatchedTitleStores}, {"unwatched_shared", testSharedTitleStores}, {"gpu_cleared", testFillClearedBeforeRange}, {"fence_reuse", testFenceReused}, {"busy_queue", testQueueBusyPastPacing}, {"allocation_ranges", testRegisteredRangeBoundaries}, {"in_flight_delta", testDeltaReadInFlight}, {"in_flight_store", testStoreIntoDeltaReadInFlight}, {"hold_diagnostics", testHoldDiagnostics}, {"late_store", testStoreAfterLastFrame}, {"wait_before_read", testWaitBeforeRead}, {"wait_after_read", testWaitAfterRead}, {"unwatched_in_flight", testUnwatchedLeftInFlight}, {"unwatched_restored", testUnwatchedRestoredInFlight}, {"host_page_restored", testHostPageRestoredInFlight}, {"in_flight_gate", testDeltaGatedOnStartedCopy}, {"wait64_after_read", testWait64AfterRead}, {"semaphore_after_read", testSemaphoreAfterRead}, {"rewind_after_read", testRewindAfterRead}, {"late_wait", testWaitAfterLastFrame}, {"wait_labelled_after", testWaitLabelledAfter}, {"wait_labelled_before", testWaitLabelledBefore}, {"wait_labelled_skipped", testWaitLabelledSkipped}, {"wait_labelled_elsewhere", testWaitLabelledElsewhere}, {"carried_label", testCarriedLabelKeepsSerial}, {"wait_labelled_rewind", testWaitLabelledRewind}, {"wait_labelled_rewind_chain", testWaitLabelledRewindChain}, {"flip_serial_start", testFlipSerialAtStart}, {"unwatched_rewatched", testUnwatchedRewatchedInFlight}, {"in_flight_once", testDeltaReadInFlightOnce}, {"owner_lifetime", testOwnerLifetime}, {"owner_lifetime_rewritten", testOwnerLifetimeRewritten}, {"empty_in_flight_read", testEmptyDeltaReadInFlight}, {"held_concurrent_store", testStoreWhileDeltaHeld}, {"shader_in_flight", testShaderDeltaReadInFlight}, {"last_thread_hold", testLastFrameEndedOnNewThread}};
    if (argc != 2 && !(argc == 3 && (mode == "concurrent" || mode == "pipelines" || mode == "reservation_error" || mode == "rewind" || rewound.contains(mode)))) {
        std::fprintf(stderr, "usage: agc_driver_frame_replay_tests <agc_frame_replay> [concurrent|pipelines|reservation_error|rewind|rewind_store|store_idle|store_idle_rewind|store_idle_unwatched|store_second_wait|rewind_copy|rewind_compute|rewind_release|rewind_fill|rewind_split|wait_label|gpu_store|gpu_fill|gpu_restore_cached|unwatched_start|unwatched_late|unwatched_shared|gpu_cleared|fence_reuse|busy_queue|allocation_ranges|in_flight_delta|in_flight_store|hold_diagnostics|late_store|wait_after_read|wait_before_read|unwatched_in_flight|unwatched_restored|host_page_restored|in_flight_gate|wait64_after_read|semaphore_after_read|rewind_after_read|late_wait|wait_labelled_after|wait_labelled_before|wait_labelled_skipped|wait_labelled_elsewhere|carried_label|wait_labelled_rewind|wait_labelled_rewind_chain|flip_serial_start|unwatched_rewatched|in_flight_once|owner_lifetime|owner_lifetime_rewritten|empty_in_flight_read|held_concurrent_store|shader_in_flight|last_thread_hold]\n");
        return 2;
    }
    int status = 0;
    try {
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
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
