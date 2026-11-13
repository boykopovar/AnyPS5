#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "execution/VulkanTestDevice.hpp"
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
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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
    const auto repeated = replay("--repeat 3 --verify-reset", capture / "repeat output.json");
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

constexpr std::uint32_t RewindHeader = 0xc0005900u;
constexpr std::size_t PageWords = 1024;

class RewindCapture {
public:
    explicit RewindCapture(const std::filesystem::path& directory) : directory(directory), writer(directory), words(BlockBytes / sizeof(std::uint32_t)) {}

    std::uint64_t address(std::size_t index) const {
        return at(layout.base) + index * sizeof(std::uint32_t);
    }

    void place(std::size_t index, std::span<const std::uint32_t> values) {
        std::copy(values.begin(), values.end(), words.begin() + static_cast<std::ptrdiff_t>(index));
    }

    std::vector<std::uint32_t> writeData(std::size_t label, std::uint32_t value) const {
        std::array<std::uint32_t, 16> encoded{};
        CommandBuffer buffer{encoded.data(), encoded.data() + encoded.size(), encoded.data(), encoded.data() + encoded.size(), nullptr, nullptr, 0};
        sceAgcDcbWriteData(&buffer, GraphicsMemory, 0, address(label), &value, 1, 0, 1);
        return {encoded.data(), buffer.cursor_up};
    }

    std::vector<std::uint32_t> chain(std::size_t target, std::size_t count) const {
        return {0xc0023f00u, static_cast<std::uint32_t>(address(target)), static_cast<std::uint32_t>(address(target) >> 32u), (1u << 20u) | static_cast<std::uint32_t>(count)};
    }

    void start() {
        writer.WriteRange(at(layout.base), BlockBytes, AgcDriver::FrameRangeKind::Watched, 0x04, AgcDriver::FrameRangeAccess::Readable | AgcDriver::FrameRangeAccess::Writable | AgcDriver::FrameRangeAccess::Gpu);
        writer.WriteSnapshot(at(layout.base), std::as_bytes(std::span(words)));
        AgcDriver::FrameCall call;
        call.kind = FrameCallKind::FrameStart;
        call.ordinal = ++ordinal;
        writer.WriteCall(call);
    }

    void submit(std::size_t index, std::size_t count) {
        AgcDriver::FrameCall call;
        call.kind = FrameCallKind::Submit;
        call.ordinal = ++ordinal;
        call.thread = 1;
        call.address = address(index);
        call.size = count;
        call.accepted = submitted;
        call.completed = submitted;
        ++submitted;
        writer.WriteCall(call);
    }

    void delta(std::uint64_t inFlight, std::initializer_list<std::pair<std::size_t, std::size_t>> pages, std::initializer_list<std::size_t> parked) {
        ++group;
        for (const auto& [first, last] : pages) writer.WriteDelta(group, submitted - inFlight, address(first * PageWords), std::as_bytes(std::span(words).subspan(first * PageWords, (last - first) * PageWords)));
        AgcDriver::FrameCall call;
        call.kind = FrameCallKind::Memory;
        call.ordinal = ++ordinal;
        call.thread = 1;
        call.accepted = submitted;
        call.completed = submitted - inFlight;
        call.values = {group, parked.size()};
        for (const auto index : parked) call.values.push_back(address(index));
        std::sort(call.values.begin() + 2, call.values.end());
        writer.WriteCall(call);
    }

    std::uint32_t replay(const std::filesystem::path& tool, std::size_t label) {
        std::uintptr_t base = 0;
        std::size_t size = 0;
        GuestArena::GuestArenaRange_nid_postfix(&base, &size);
        writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
        const auto dump = directory / "label.bin";
        const auto log = directory / "replay.log";
        std::filesystem::remove(dump);
        auto command = "\"" + tool.string() + "\" \"" + directory.string() + "\" --strict --dump " + std::to_string(address(label)) + ":4:\"" + dump.string() + "\" > \"" + (directory / "replay.json").string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
        command = "\"" + command + "\"";
#endif
        const auto code = std::system(command.c_str());
        const auto errors = readFile(log);
        check(code == 0, "agc_frame_replay of " + directory.filename().string() + " exited with " + std::to_string(code) + ": " + std::string(reinterpret_cast<const char*>(errors.data()), errors.size()));
        const auto bytes = readFile(dump);
        check(bytes.size() == sizeof(std::uint32_t), "the replay dumped " + std::to_string(bytes.size()) + " bytes of the label");
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }

private:
    std::filesystem::path directory;
    Layout layout = allocate();
    AgcDriver::FrameCaptureWriter writer;
    std::vector<std::uint32_t> words;
    std::uint64_t ordinal = 0;
    std::uint64_t group = 0;
    std::uint64_t submitted = 0;
};

std::filesystem::path rewindCaptureDirectory() {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    return directory;
}

void testChainedRewindGates(const std::filesystem::path& tool) {
    RewindCapture capture(rewindCaptureDirectory());
    constexpr std::size_t Label = 3 * PageWords;
    constexpr std::size_t Segment = 2 * PageWords;
    capture.place(0, std::array{RewindHeader, 0u, 0u, 0u, 0u, 0u, 0u, 0u});
    capture.start();
    capture.submit(0, 8);
    const auto write = capture.writeData(Label, Done);
    capture.place(1, std::array{AgcDriver::Pm4::RewindValid, RewindHeader, AgcDriver::Pm4::RewindValid});
    capture.place(4, capture.chain(Segment, write.size()));
    capture.place(Segment, write);
    capture.delta(0, {{0, 1}, {2, 3}}, {});
    check(capture.replay(tool, Label) == Done, "the replay did not run the segment behind the second of two REWINDs released by one memory group");
}

void testRewindControlAtPageStart(const std::filesystem::path& tool) {
    RewindCapture capture(rewindCaptureDirectory());
    constexpr std::size_t Writes = 1000;
    constexpr std::size_t Control = 5 * PageWords;
    constexpr std::size_t Scratch = 7 * PageWords;
    constexpr std::size_t Label = 8 * PageWords;
    const auto busy = capture.writeData(Scratch, Done);
    const auto start = Control - 1 - Writes * busy.size();
    for (std::size_t index = 0; index < Writes; ++index) capture.place(start + index * busy.size(), busy);
    capture.place(Control - 1, std::array{RewindHeader});
    capture.start();
    const auto write = capture.writeData(Label, Done);
    capture.submit(start, Control + 1 + write.size() - start);
    capture.place(Control, std::array{AgcDriver::Pm4::RewindValid});
    capture.place(Control + 1, write);
    capture.delta(1, {{5, 6}}, {Control});
    check(capture.replay(tool, Label) == Done, "the replay did not run the tail of a REWIND whose control word starts a memory chunk");
}

void testZeroRewindControlRewritten(const std::filesystem::path& tool) {
    RewindCapture capture(rewindCaptureDirectory());
    constexpr std::size_t Label = 3 * PageWords;
    const auto write = capture.writeData(Label, Done);
    capture.place(0, std::array{RewindHeader, 0u});
    capture.start();
    capture.submit(0, 2 + write.size());
    capture.place(2, write);
    capture.delta(1, {{0, 1}}, {1});
    capture.place(1, std::array{AgcDriver::Pm4::RewindValid});
    capture.delta(0, {{0, 1}}, {});
    check(capture.replay(tool, Label) == Done, "the replay did not run the tail of a REWIND whose block was rewritten while its control stayed zero");
}

void testRewindControlsKeptAnywhere(const std::filesystem::path& tool) {
    RewindCapture capture(rewindCaptureDirectory());
    constexpr std::size_t Last = 4 * PageWords - 1;
    capture.start();
    std::vector<std::uint32_t> image(4 * PageWords);
    for (const auto header : {std::size_t{0}, std::size_t{14}, std::size_t{15}, std::size_t{100}, std::size_t{101}, PageWords - 2, PageWords - 1, 3 * PageWords - 1, Last - 1}) image[header] = RewindHeader;
    image[40] = 0xc0ff5902u;
    std::size_t decoy = 200;
    for (const auto word : {0x00590000u, 0x80005900u, 0xc0005a00u, 0x0000c059u, 0x59c00000u}) image[decoy += 2] = word;
    for (std::size_t index = 1; index < image.size(); ++index) {
        if (image[index] == 0 && image[index - 1] != 0) image[index] = AgcDriver::Pm4::RewindValid | static_cast<std::uint32_t>(index);
    }
    capture.place(0, image);
    std::size_t expected = 0;
    for (const auto& [first, last] : {std::pair{std::size_t{0}, PageWords}, std::pair{2 * PageWords, 4 * PageWords}}) {
        ++expected;
        for (auto index = first + 1; index < last; ++index) expected += (image[index - 1] >> 30u) == 3u && ((image[index - 1] >> 8u) & 0xffu) == 0x59u;
    }
    capture.delta(0, {{0, 1}, {2, 4}}, {});
    const auto control = capture.replay(tool, Last);
    check(control == image[Last], "the replay left " + std::to_string(control) + " in the REWIND control that ends a memory chunk");
    const auto json = readFile(rewindCaptureDirectory() / "replay.json");
    const std::string line(reinterpret_cast<const char*>(json.data()), json.size());
    constexpr std::string_view Field = "\"kept_words\":";
    const auto field = line.find(Field);
    check(field != std::string::npos, "the replay JSON has no kept_words");
    const auto kept = std::strtoull(line.c_str() + field + Field.size(), nullptr, 10);
    check(kept == expected, "the replay stored " + std::to_string(kept) + " control words after their memory group, expected " + std::to_string(expected));
}

void testEarlyReleaseRestoredInOrder(const std::filesystem::path& tool) {
    RewindCapture capture(rewindCaptureDirectory());
    constexpr std::size_t Label = 3 * PageWords;
    constexpr std::uint32_t Reset = 2;
    const auto write = capture.writeData(Label, Done);
    capture.place(0, std::array{RewindHeader, 0u});
    capture.start();
    capture.submit(0, 2 + write.size());
    capture.place(1, std::array{AgcDriver::Pm4::RewindValid});
    capture.place(2, write);
    capture.place(Label, std::array{Reset});
    capture.delta(0, {{0, 1}, {3, 4}}, {});
    const auto label = capture.replay(tool, Label);
    check(label == Reset, "the replay left " + std::to_string(label) + " in a label that the group released early holds as " + std::to_string(Reset) + " after the tail wrote " + std::to_string(Done));
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

constexpr std::uint32_t DepthWidth = 128;
constexpr std::uint32_t DepthHeight = 64;
constexpr std::uint32_t DepthSamplesLog2 = 2;
constexpr std::size_t HtileOffset = 64u * 1024u;
constexpr std::size_t PageBytes = 4096;
constexpr std::size_t PlaneBytes = static_cast<std::size_t>(DepthWidth) * DepthHeight * (1u << DepthSamplesLog2) * sizeof(float);
constexpr std::size_t CopyTexels = DepthWidth * DepthHeight;
constexpr std::size_t CopyCount = 5;
constexpr std::size_t CopyBytes = CopyCount * CopyTexels * sizeof(float);
constexpr std::uint32_t StandardLocations4x = 0x622ae6aeu;
constexpr float DrawnDepth = 0.25f;
constexpr float ClearedDepth = 0.0f;
constexpr float Untouched = -1.0f;
constexpr std::array<std::uint32_t, 16> DepthVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
constexpr std::array<std::uint32_t, 4> DepthPixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};

struct ShaderHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<ShaderRegister, 4> registers{};
};

const std::uint32_t* registerShader(std::byte* memory, std::span<const std::uint32_t> code, std::uint8_t type) {
    auto* const words = reinterpret_cast<std::uint32_t*>(memory);
    std::copy(code.begin(), code.end(), words);
    auto* const header = new (memory + 256) ShaderHeader{};
    header->shader.file_header = 0x34333231u;
    header->shader.version = 0x18u;
    header->shader.header_size = sizeof(ShaderHeader);
    header->shader.shader_size = static_cast<std::uint32_t>(code.size_bytes());
    header->shader.code = words;
    header->shader.type = type;
    header->shader.user_data = &header->userData;
    if (type == 1) {
        header->registers = {{{0x008, static_cast<std::uint32_t>(at(words) >> 8u)}, {0x009, static_cast<std::uint32_t>(at(words) >> 40u)}, {0x00b, 0}, {0x00a, 0}}};
        header->shader.sh_registers = header->registers.data();
        header->shader.num_sh_registers = static_cast<std::uint8_t>(header->registers.size());
    }
    AgcDriverRegisterShader_nid_postfix(&header->shader);
    return words;
}

void appendRegister(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    commands.insert(commands.end(), {0xc0010000u | (opcode << 8u), offset, value});
}

void appendDepthDraw(std::vector<std::uint32_t>& commands, const std::uint32_t* vertex, const std::uint32_t* pixel, const float* color, std::uint64_t depth, std::uint64_t htile, bool depthCopy, std::uint32_t copySample = 0, std::uint32_t samplesLog2 = DepthSamplesLog2) {
    const auto colorAddress = at(color);
    const bool multisampled = samplesLog2 != 0u;
    const auto locations = depthCopy || !multisampled ? 0u : StandardLocations4x;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b3, 0x2}, {0x1b4, 0x2}, {0x1b6, 0}, {0x207, 0}, {0x200, depthCopy ? 0u : 0x76u}, {0x203, 0},
        {0x2dc, 0xaa00}, {0x2f8, depthCopy || !multisampled ? 0u : 0x0020c002u}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, depthCopy ? 1u : 0u}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 1}, {0x1c3, 4}, {0x31c, 0x8710},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, ((DepthWidth - 1u) << 14u) | (DepthHeight - 1u)},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, (DepthHeight << 16u) | DepthWidth},
        {0x81, 0x80000000}, {0x82, (DepthHeight << 16u) | DepthWidth},
        {0x90, 0x80000000}, {0x91, (DepthHeight << 16u) | DepthWidth},
        {0x94, 0x80000000}, {0x95, (DepthHeight << 16u) | DepthWidth},
        {0x10f, std::bit_cast<std::uint32_t>(DepthWidth / 2.0f)}, {0x110, std::bit_cast<std::uint32_t>(DepthWidth / 2.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-(DepthHeight / 2.0f))}, {0x112, std::bit_cast<std::uint32_t>(DepthHeight / 2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, std::bit_cast<std::uint32_t>(DrawnDepth)},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, locations}, {0x302, locations}, {0x306, locations}, {0x30a, locations},
        {0x318, static_cast<std::uint32_t>(colorAddress >> 8u)}, {0x390, static_cast<std::uint32_t>(colorAddress >> 40u)}, {0x292, depthCopy ? 2u : 3u},
        {0x000, depthCopy ? (copySample == 0 ? 0x84u : 4u | (copySample << 8u)) : 0u}, {0x002, 0}, {0x007, ((DepthHeight - 1u) << 16u) | (DepthWidth - 1u)},
        {0x008, 0}, {0x009, std::bit_cast<std::uint32_t>(1.0f)}, {0x00a, 0}, {0x00b, 0},
        {0x010, 3u | (samplesLog2 << 2u) | (24u << 4u) | (1u << 20u) | (1u << 29u)}, {0x011, 1u << 29u}, {0x2af, 1u << 18u},
        {0x012, static_cast<std::uint32_t>(depth >> 8u)}, {0x014, static_cast<std::uint32_t>(depth >> 8u)},
        {0x01a, static_cast<std::uint32_t>(depth >> 40u)}, {0x01c, static_cast<std::uint32_t>(depth >> 40u)},
        {0x005, static_cast<std::uint32_t>(htile >> 8u)}, {0x01e, static_cast<std::uint32_t>(htile >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(at(vertex) >> 8u)}, {0xc9, static_cast<std::uint32_t>(at(vertex) >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(at(pixel) >> 8u)}, {0x009, static_cast<std::uint32_t>(at(pixel) >> 40u)}, {0x00a, 0}, {0x00b, 0},
    };
    appendRegister(commands, 0x79, 0x242, 4);
    for (const auto& [offset, value] : context) appendRegister(commands, 0x69, offset, value);
    for (const auto& [offset, value] : shader) appendRegister(commands, 0x76, offset, value);
    commands.insert(commands.end(), {0xc0012d00u, 3u, 2u});
}

std::vector<std::byte> snapshotOf(const FrameCaptureReader& reader, std::uint64_t address, std::size_t bytes) {
    std::vector<std::byte> result(bytes);
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Snapshot || chunk.address + chunk.bytes <= address || address + bytes <= chunk.address) continue;
        const auto piece = reader.Read(chunk);
        const auto begin = std::max(chunk.address, address);
        const auto end = std::min(chunk.address + chunk.bytes, address + bytes);
        std::memcpy(result.data() + (begin - address), piece.data() + (begin - chunk.address), static_cast<std::size_t>(end - begin));
    }
    return result;
}

std::vector<std::uint64_t> passValues(const std::string& report, const std::string& name) {
    std::vector<std::uint64_t> values;
    const auto key = "\"" + name + "\":";
    for (auto found = report.find(key); found != std::string::npos; found = report.find(key, found + 1)) values.push_back(std::strtoull(report.c_str() + found + key.size(), nullptr, 10));
    return values;
}

void testSingleSampleDepthPasses(const std::filesystem::path& tool) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const auto layout = allocate();
    const auto planes = allocate();
    auto* const target = reinterpret_cast<float*>(layout.source);
    auto* const later = target + CopyTexels;
    constexpr std::size_t Copies = 2;
    constexpr std::size_t Bytes = Copies * CopyTexels * sizeof(float);
    std::fill(target, target + Copies * CopyTexels, Untouched);
    auto* const shaders = reinterpret_cast<std::byte*>(layout.secondCommands);
    const auto* const vertex = registerShader(shaders, DepthVertexCode, 2);
    const auto* const pixel = registerShader(shaders + 1024, DepthPixelCode, 1);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    std::vector<std::uint32_t> commands;
    appendDepthDraw(commands, vertex, pixel, target, at(planes.base), at(planes.base + HtileOffset), true, 0, 0);
    appendDepthDraw(commands, vertex, pixel, target, at(planes.base), at(planes.base + HtileOffset), false, 0, 0);
    appendDepthDraw(commands, vertex, pixel, later, at(planes.base), at(planes.base + HtileOffset), true, 0, 0);
    std::copy(commands.begin(), commands.end(), layout.firstCommands);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the single-sample depth draw and its DB depth copies were not submitted");
    flip();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(at(target), Bytes);
    std::vector<std::byte> drawn(Bytes);
    std::memcpy(drawn.data(), target, Bytes);
    std::size_t cleared = 0;
    std::size_t covered = 0;
    for (std::size_t index = 0; index < CopyTexels; ++index) {
        check(target[index] == ClearedDepth || target[index] == Untouched, "the single-sample DB depth copy before the draw holds " + std::to_string(target[index]) + " at texel " + std::to_string(index));
        check(later[index] == DrawnDepth || later[index] == ClearedDepth || later[index] == Untouched, "the single-sample DB depth copy after the draw holds " + std::to_string(later[index]) + " at texel " + std::to_string(index));
        cleared += target[index] == ClearedDepth;
        covered += later[index] == DrawnDepth;
    }
    check(cleared != 0, "the single-sample DB depth copy before the draw did not read the cleared depth");
    check(covered != 0 && covered != CopyTexels, "the single-sample depth triangle covered " + std::to_string(covered) + " texels, expected an edge through the target");
    {
        std::exception_ptr failure;
        AgcDriverFlushGuestMemory_nid_no_patch(at(planes.base), BlockBytes, &failure);
        if (failure) std::rethrow_exception(failure);
        std::size_t owners = 1;
        AgcDriverResidentOwnersOver_nid_no_patch(at(planes.base), BlockBytes, true, &owners, &failure);
        if (failure) std::rethrow_exception(failure);
        check(owners == 0, "the reset flush left " + std::to_string(owners) + " GPU owners over the single-sample depth plane");
        const auto* const plane = reinterpret_cast<const float*>(planes.base);
        check(std::count(plane, plane + CopyTexels, DrawnDepth) != 0, "the reset flush did not store the drawn single-sample depth into guest memory");
    }
    AgcDriverShutdown_nid_postfix();

    const auto dump = capture / "single sample depth copy.bin";
    std::filesystem::remove(dump);
    const auto result = capture / "single sample depth.json";
    const auto log = capture / "single sample depth.log";
    auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --verify-reset --repeat 2 --dump " + std::to_string(at(target)) + ":" + std::to_string(Bytes) + ":\"" + dump.string() + "\" > \"" + result.string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto code = std::system(command.c_str());
    const auto errors = readFile(log);
    check(code == 0, "agc_frame_replay --repeat 2 of the single-sample depth frame exited with " + std::to_string(code) + ": " + std::string(reinterpret_cast<const char*>(errors.data()), errors.size()));
    const auto copies = readFile(dump);
    check(copies.size() == drawn.size(), "the replay dumped " + std::to_string(copies.size()) + " bytes of the single-sample DB depth copies");
    for (std::size_t index = 0; index < Copies * CopyTexels; ++index) {
        float expected = 0.0f;
        float found = 0.0f;
        std::memcpy(&expected, drawn.data() + index * sizeof(float), sizeof(float));
        std::memcpy(&found, copies.data() + index * sizeof(float), sizeof(float));
        check(std::memcmp(&expected, &found, sizeof(float)) == 0, "pass 2 left " + std::to_string(found) + " in single-sample DB depth copy " + std::to_string(index / CopyTexels) + " at texel " + std::to_string(index % CopyTexels) + ", captured " + std::to_string(expected));
    }
}

void testMultisampleDepthPasses(const std::filesystem::path& tool) {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    const std::filesystem::path capture(directory);
    const auto layout = allocate();
    check(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536) != nullptr, "cannot reserve the guest memory between the copy target and the depth planes");
    const auto planeLow = allocate();
    const auto planeHigh = allocate();
    check(planeHigh.base == planeLow.base + BlockBytes, "the two blocks holding the depth planes are not adjacent");
    auto* const planeBase = planeLow.base + BlockBytes - PlaneBytes / 2u;
    auto* const htileBase = planeHigh.base + HtileOffset;
    auto* const target = reinterpret_cast<float*>(layout.source);
    auto* const later = target + CopyTexels;
    std::fill(target, target + CopyCount * CopyTexels, Untouched);
    auto* const shaders = reinterpret_cast<std::byte*>(layout.secondCommands);
    const auto* const vertex = registerShader(shaders, DepthVertexCode, 2);
    const auto* const pixel = registerShader(shaders + 1024, DepthPixelCode, 1);
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);

    flip();
    std::vector<std::uint32_t> commands;
    appendDepthDraw(commands, vertex, pixel, target, at(planeBase), at(htileBase), true);
    appendDepthDraw(commands, vertex, pixel, target, at(planeBase), at(htileBase), false);
    appendDepthDraw(commands, vertex, pixel, later, at(planeBase), at(htileBase), true);
    for (std::uint32_t sample = 1; sample < 4; ++sample) appendDepthDraw(commands, vertex, pixel, later + sample * CopyTexels, at(planeBase), at(htileBase), true, sample);
    check(commands.size() * sizeof(std::uint32_t) <= CommandBytes, "the multisample depth commands do not fit their buffer");
    std::copy(commands.begin(), commands.end(), layout.firstCommands);
    Packet packet{layout.firstCommands, static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the multisample depth draw and its DB depth copies were not submitted");
    flip();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(at(target), CopyBytes);
    AgcDriver::GuestMemory::FlushGpuWrites(at(planeLow.base), 2u * BlockBytes);
    std::vector<std::byte> drawn(CopyBytes);
    std::memcpy(drawn.data(), target, CopyBytes);
    std::size_t cleared = 0;
    std::size_t covered = 0;
    for (std::size_t index = 0; index < CopyTexels; ++index) {
        check(target[index] == ClearedDepth || target[index] == Untouched, "the DB depth copy before the draw holds " + std::to_string(target[index]) + " at texel " + std::to_string(index));
        check(later[index] == DrawnDepth || later[index] == ClearedDepth || later[index] == Untouched, "the DB depth copy after the draw holds " + std::to_string(later[index]) + " at texel " + std::to_string(index));
        cleared += target[index] == ClearedDepth;
        covered += later[index] == DrawnDepth;
    }
    check(cleared != 0, "the DB depth copy before the draw did not read the cleared depth");
    check(covered != 0 && covered != CopyTexels, "the multisample depth triangle covered " + std::to_string(covered) + " texels, expected an edge through the target");
    bool samplesDiffer = false;
    for (std::size_t copy = 2; copy < CopyCount; ++copy) {
        std::size_t drawnTexels = 0;
        for (std::size_t index = 0; index < CopyTexels; ++index) {
            const auto value = target[copy * CopyTexels + index];
            check(value == DrawnDepth || value == ClearedDepth || value == Untouched, "the DB depth copy of sample " + std::to_string(copy - 1) + " holds " + std::to_string(value) + " at texel " + std::to_string(index));
            drawnTexels += value == DrawnDepth;
            samplesDiffer |= value != later[index];
        }
        check(drawnTexels != 0, "the DB depth copy of sample " + std::to_string(copy - 1) + " holds no drawn depth");
    }
    check(samplesDiffer, "the DB depth copies of samples 1 to 3 equal the copy of sample 0 at every texel");
    const std::array<std::pair<const Layout*, std::vector<std::byte>>, 3> finals{{
        {&layout, std::vector<std::byte>(layout.base, layout.base + BlockBytes)},
        {&planeLow, std::vector<std::byte>(planeLow.base, planeLow.base + BlockBytes)},
        {&planeHigh, std::vector<std::byte>(planeHigh.base, planeHigh.base + BlockBytes)},
    }};
    std::vector<std::pair<std::uint64_t, std::uint64_t>> hostOnly;
    std::exception_ptr failure;
    AgcDriverGpuOwnedRanges_nid_no_patch(at(planeLow.base), 2u * BlockBytes, hostOnly, &failure);
    if (failure) std::rethrow_exception(failure);
    check(!hostOnly.empty(), "the multisample depth draw left no samples only on the host");
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the multisample depth commands were not submitted again");
    struct Probe {
        bool contended = true;
        std::uint64_t accepted = 0;
        std::uint64_t completed = 1;
    } probe;
    AgcDriverGuestMemoryTransaction_nid_no_patch([](void* context) {
        auto& inside = *static_cast<Probe*>(context);
        AgcDriverSerialProgress_nid_postfix(&inside.accepted, &inside.completed);
        inside.contended = std::async(std::launch::async, [] {
            std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex(), std::try_to_lock);
            return lock.owns_lock();
        }).get();
    }, &probe, &failure);
    if (failure) std::rethrow_exception(failure);
    check(probe.accepted == probe.completed, "the GPU had work in flight inside a guest memory transaction");
    check(!probe.contended, "another thread took the GPU lock inside a guest memory transaction");
    static Packet* late = nullptr;
    static int lateResult = -1;
    late = &packet;
    AgcDriverSetGuestMemoryTransactionHookForTest_nid_no_patch([] { lateResult = sceAgcDriverSubmitDcb(late); });
    AgcDriverGuestMemoryTransaction_nid_no_patch([](void*) {}, nullptr, &failure);
    AgcDriverSetGuestMemoryTransactionHookForTest_nid_no_patch(nullptr);
    check(lateResult == 0, "the submission made inside a guest memory transaction was not accepted");
    std::string rejection;
    try {
        if (failure) std::rethrow_exception(failure);
    } catch (const std::runtime_error& error) {
        rejection = error.what();
    }
    check(rejection.find("accepted during a guest memory transaction") != std::string::npos, "a guest memory transaction ended without failing although a submission was accepted inside it: " + rejection);
    AgcDriverWaitIdle_nid_postfix();
    failure = nullptr;
    {
        std::lock_guard held(AgcDriver::GuestMemory::GpuMutex());
        AgcDriverGuestMemoryTransaction_nid_no_patch([](void*) {}, nullptr, &failure);
    }
    rejection.clear();
    try {
        if (failure) std::rethrow_exception(failure);
    } catch (const std::runtime_error& error) {
        rejection = error.what();
    }
    check(rejection.find("with the GPU lock held") != std::string::npos, "a guest memory transaction entered with the GPU lock held did not fail: " + rejection);
    failure = nullptr;
    check(std::async(std::launch::async, [] {
        std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex(), std::try_to_lock);
        return lock.owns_lock();
    }).get(), "a guest memory transaction kept the GPU lock after it ended");
    AgcDriverShutdown_nid_postfix();

    const FrameCaptureReader reader(capture);
    std::size_t expectedPages = 0;
    std::size_t heldPages = 0;
    std::size_t heldMatching = 0;
    for (const auto& [block, bytes] : finals) {
        const auto snapshot = snapshotOf(reader, at(block->base), BlockBytes);
        for (std::size_t offset = 0; offset < BlockBytes; offset += PageBytes) {
            const auto page = at(block->base) + offset;
            const bool held = std::any_of(hostOnly.begin(), hostOnly.end(), [&](const auto& range) { return range.first < page + PageBytes && page < range.first + range.second; });
            const bool differs = std::memcmp(bytes.data() + offset, snapshot.data() + offset, PageBytes) != 0;
            expectedPages += differs;
            heldPages += held;
            heldMatching += held && !differs;
        }
    }
    check(heldMatching != 0, "every page of the multisample depth owners differs from the snapshot, so an owner retired without a rewrite is not exercised");

    const auto dump = capture / "multisample depth copy.bin";
    const auto replay = [&](std::size_t passes) {
        std::filesystem::remove(dump);
        const auto result = capture / ("multisample depth " + std::to_string(passes) + ".json");
        const auto log = capture / ("multisample depth " + std::to_string(passes) + ".log");
        auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --verify-reset --repeat " + std::to_string(passes) + " --dump " + std::to_string(at(target)) + ":" + std::to_string(CopyBytes) + ":\"" + dump.string() + "\" > \"" + result.string() + "\" 2> \"" + log.string() + "\"";
#ifdef _WIN32
        command = "\"" + command + "\"";
#endif
        const auto code = std::system(command.c_str());
        const auto errors = readFile(log);
        check(code == 0, "agc_frame_replay --repeat " + std::to_string(passes) + " exited with " + std::to_string(code) + ": " + std::string(reinterpret_cast<const char*>(errors.data()), errors.size()));
        const auto copies = readFile(dump);
        check(copies.size() == drawn.size(), "the replay with " + std::to_string(passes) + " passes dumped " + std::to_string(copies.size()) + " bytes of the DB depth copies");
        for (std::size_t index = 0; index < CopyCount * CopyTexels; ++index) {
            float expected = 0.0f;
            float found = 0.0f;
            std::memcpy(&expected, drawn.data() + index * sizeof(float), sizeof(float));
            std::memcpy(&found, copies.data() + index * sizeof(float), sizeof(float));
            check(std::memcmp(&expected, &found, sizeof(float)) == 0, "the replay with " + std::to_string(passes) + " passes left " + std::to_string(found) + " in DB depth copy " + std::to_string(index / CopyTexels) + " at texel " + std::to_string(index % CopyTexels) + ", captured " + std::to_string(expected));
        }
        const auto json = readFile(result);
        const std::string report(reinterpret_cast<const char*>(json.data()), json.size());
        std::size_t replayed = 0;
        for (auto found = report.find("\"frames\":1,\"p50\""); found != std::string::npos; found = report.find("\"frames\":1,\"p50\"", found + 1)) ++replayed;
        check(replayed == passes, "the replay did not replay the frame in each of " + std::to_string(passes) + " passes: " + report);
        if (passes < 2) return;
        const auto rewritten = passValues(report, "rewritten_bytes");
        const auto captured = passValues(report, "range_bytes");
        check(rewritten.size() == passes && captured.size() == passes && rewritten[0] == 0, "the replay report lists " + std::to_string(rewritten.size()) + " passes with rewritten bytes: " + report);
        const auto workers = passValues(report, "reset_workers");
        const auto retiredDepth = passValues(report, "retired_depth_owners");
        check(workers.size() == passes && retiredDepth.size() == passes, "the replay report lists " + std::to_string(workers.size()) + " passes with reset workers: " + report);
        for (std::size_t pass = 1; pass < passes; ++pass) {
            check(std::thread::hardware_concurrency() < 2 || workers[pass] >= 2, "pass " + std::to_string(pass + 1) + " compared the captured ranges on " + std::to_string(workers[pass]) + " threads");
            check(rewritten[pass] == expectedPages * PageBytes, "pass " + std::to_string(pass + 1) + " rewrote " + std::to_string(rewritten[pass]) + " of " + std::to_string(captured[pass]) + " captured bytes, expected exactly the " + std::to_string(expectedPages) + " pages that differ from the snapshot (" + std::to_string(heldPages) + " pages hold GPU owners, " + std::to_string(heldMatching) + " of them unchanged)");
            check(retiredDepth[pass] != 0, "pass " + std::to_string(pass + 1) + " retired no native depth owner although the earlier pass left samples only on the GPU");
        }
    };
    replay(1);
    replay(2);
    replay(3);
    for (const auto& [fault, expected] : {std::pair<std::string, std::string>{"owners", "survived the reset"}, {"omit", "survived the reset"}, {"bytes", "differs from the captured contents"}, {"remap", "mapped or unmapped during the reset"}}) {
        const auto faultLog = capture / ("multisample depth fault " + fault + ".log");
#ifdef _WIN32
        _putenv_s("APS5_REPLAY_RESET_FAULT_FOR_TEST", fault.c_str());
#else
        setenv("APS5_REPLAY_RESET_FAULT_FOR_TEST", fault.c_str(), 1);
#endif
        auto command = "\"" + tool.string() + "\" \"" + capture.string() + "\" --strict --verify-reset --repeat 2 > \"" + (capture / "multisample depth fault.json").string() + "\" 2> \"" + faultLog.string() + "\"";
#ifdef _WIN32
        command = "\"" + command + "\"";
#endif
        const auto code = std::system(command.c_str());
#ifdef _WIN32
        _putenv_s("APS5_REPLAY_RESET_FAULT_FOR_TEST", "");
#else
        unsetenv("APS5_REPLAY_RESET_FAULT_FOR_TEST");
#endif
        const auto errors = readFile(faultLog);
        const std::string text(reinterpret_cast<const char*>(errors.data()), errors.size());
        check(code != 0 && text.find(expected) != std::string::npos, "--verify-reset accepted a reset made faulty by " + fault + " (exit " + std::to_string(code) + "): " + text);
    }
#ifdef __linux__
    const auto refusedLog = capture / "multisample depth unwatched.log";
    const auto refused = std::system(("APS5_NO_WRITE_WATCH=1 \"" + tool.string() + "\" \"" + capture.string() + "\" --strict > /dev/null 2> \"" + refusedLog.string() + "\"").c_str());
    const auto refusal = readFile(refusedLog);
    check(refused != 0 && std::string(reinterpret_cast<const char*>(refusal.data()), refusal.size()).find("write watch does not cover") != std::string::npos, "the replay accepted a watched range without the guest write watch");
#endif
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

}

int main(int argc, char** argv) {
    const std::string mode = argc == 3 ? argv[2] : "";
    const std::map<std::string, void (*)(const std::filesystem::path&)> rewound{
        {"rewind_store", testStoreAfterGatedCopy}, {"rewind_copy", testCopyInFlightAtDelta}, {"rewind_compute", testComputeWaitBehindRewind}, {"rewind_release", testReleaseDuringDelta}, {"rewind_fill", testFilledRewindTail}, {"rewind_split", testRewindFilledAcrossChunks}, {"wait_label", testLabelFromLaterSubmission}, {"fence_reuse", testFenceReused}, {"busy_queue", testQueueBusyPastPacing}, {"allocation_ranges", testRegisteredRangeBoundaries}};
    const std::map<std::string, void (*)(const std::filesystem::path&)> restored{
        {"rewind_chained", testChainedRewindGates}, {"rewind_page_start", testRewindControlAtPageStart}, {"rewind_zero_control", testZeroRewindControlRewritten}, {"rewind_in_order", testEarlyReleaseRestoredInOrder}, {"rewind_kept_words", testRewindControlsKeptAnywhere}, {"multisample_depth", testMultisampleDepthPasses}, {"single_sample_depth", testSingleSampleDepthPasses}};
    if (argc != 2 && !(argc == 3 && (mode == "concurrent" || mode == "pipelines" || mode == "reservation_error" || mode == "rewind" || rewound.contains(mode) || restored.contains(mode)))) {
        std::fprintf(stderr, "usage: agc_driver_frame_replay_tests <agc_frame_replay> [concurrent|pipelines|reservation_error|rewind|rewind_store|rewind_copy|rewind_compute|rewind_release|rewind_fill|rewind_split|wait_label|fence_reuse|busy_queue|rewind_chained|rewind_page_start|rewind_zero_control|rewind_in_order|rewind_kept_words|multisample_depth|single_sample_depth|allocation_ranges]\n");
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
        else if (const auto test = restored.find(mode); test != restored.end()) test->second(argv[1]);
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
