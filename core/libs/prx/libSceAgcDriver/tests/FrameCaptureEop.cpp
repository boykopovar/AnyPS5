#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using AgcDriver::FrameCallKind;
using AgcDriver::FrameCaptureReader;
using AgcDriver::FrameCaptureWriter;
using AgcDriver::FrameMemoryKind;

constexpr std::uint32_t Handle = 7;
constexpr std::size_t BlockBytes = 1u << 20;
constexpr std::size_t Frames = 6;
constexpr std::size_t CapturedFrames = 3;

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
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

class HeldOutput final : public AgcDriver::IVideoOutput {
public:
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return std::make_shared<Request>(ready); }
    void WaitForFlipRoom() override {
        std::unique_lock lock(mutex);
        waiting = true;
        changed.notify_all();
        changed.wait(lock, [&] { return room; });
    }
    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
    }
    bool WaitUntilHeld() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(10), [&] { return waiting; });
    }
    void Open() {
        {
            std::lock_guard lock(mutex);
            room = true;
        }
        changed.notify_all();
    }
    std::atomic<int> ready = 0;

private:
    std::mutex mutex;
    std::condition_variable changed;
    bool waiting = false;
    bool room = false;
};

std::byte* allocate() {
    auto* const block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
    check(block != nullptr, "cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04, BlockBytes);
    GuestAllocations::Mutation mutation;
    mutation.Add(block, BlockBytes, true, true, true);
    return block;
}

void submitEopFrame(std::byte* block, std::size_t frame) {
    const auto token = AgcDriverReserveEopFlip_nid_postfix(AgcDriver::FlipInfo{Handle, 0, 1, static_cast<std::int64_t>(frame)});
    check(token != 0, "the EOP flip of frame " + std::to_string(frame) + " was not reserved");
    auto* const words = reinterpret_cast<std::uint32_t*>(block + frame * 256);
    const auto label = reinterpret_cast<std::uintptr_t>(block + BlockBytes / 2 + frame * 8);
    const std::array<std::uint32_t, 8> release{0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), 1u, 0u, token};
    std::memcpy(words, release.data(), sizeof(release));
    Packet packet{words, static_cast<std::uint32_t>(release.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the EOP submission of frame " + std::to_string(frame) + " failed");
}

std::uint32_t* submitRewound(std::byte* block, std::size_t frame) {
    const auto token = AgcDriverReserveEopFlip_nid_postfix(AgcDriver::FlipInfo{Handle, 0, 1, static_cast<std::int64_t>(frame)});
    check(token != 0, "the EOP flip of frame " + std::to_string(frame) + " was not reserved");
    auto* const words = reinterpret_cast<std::uint32_t*>(block + frame * 256);
    const auto label = reinterpret_cast<std::uintptr_t>(block + BlockBytes / 2 + frame * 8);
    const std::array<std::uint32_t, 10> commands{0xc0005900u, 0u, 0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), 1u, 0u, token};
    std::memcpy(words, commands.data(), sizeof(commands));
    Packet packet{words, static_cast<std::uint32_t>(commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the rewound submission of frame " + std::to_string(frame) + " failed");
    return &words[1];
}

void release(std::uint32_t* control) {
    std::atomic_ref<std::uint32_t>(*control).store(AgcDriver::Pm4::RewindValid, std::memory_order_release);
}

void waitPresented(const Output& output, int presented) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (output.ready.load() < presented) {
        check(std::chrono::steady_clock::now() < deadline, "the rewind tail of frame " + std::to_string(presented - 1) + " did not flip");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void submitRewindFrame(std::byte* block, std::size_t frame, const Output& output) {
    const auto presented = output.ready.load();
    release(submitRewound(block, frame));
    waitPresented(output, presented + 1);
}

const char* captureDirectory() {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    return directory;
}

void checkCapturedFrames(const char* directory) {
    const FrameCaptureReader reader{std::filesystem::path(directory)};
    check(!reader.Truncated(), "the capture is truncated");
    check(reader.Header().find("\"frames\":" + std::to_string(CapturedFrames)) != std::string::npos, "the capture header does not record " + std::to_string(CapturedFrames) + " frames: " + reader.Header());
    const auto& calls = reader.Calls();
    const auto start = std::find_if(calls.begin(), calls.end(), [](const auto& call) { return call.kind == FrameCallKind::FrameStart; });
    check(start != calls.end(), "the capture has no frame start");
    std::size_t submits = 0, reservations = 0;
    FrameCallKind first = FrameCallKind::End;
    for (auto call = start + 1; call != calls.end(); ++call) {
        if (call->kind == FrameCallKind::Submit || call->kind == FrameCallKind::ReserveEopFlip) {
            if (first == FrameCallKind::End) first = call->kind;
            submits += call->kind == FrameCallKind::Submit;
            reservations += call->kind == FrameCallKind::ReserveEopFlip;
        }
    }
    check(first == FrameCallKind::ReserveEopFlip, "the capture started between a reservation and the submission that flips it");
    check(submits == CapturedFrames && reservations == CapturedFrames,
          "the captured frames hold " + std::to_string(submits) + " submissions and " + std::to_string(reservations) + " reservations, expected " + std::to_string(CapturedFrames) + " of each");
}

void testEopFramesCountOnce() {
    const char* directory = captureDirectory();
    auto* const block = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    for (std::size_t frame = 0; frame < Frames; ++frame) submitEopFrame(block, frame);
    AgcDriverWaitIdle_nid_postfix();
    check(output->ready.load() == static_cast<int>(Frames), "the driver presented " + std::to_string(output->ready.load()) + " of the EOP flips");
    checkCapturedFrames(directory);
}

void testRewindTailFramesCount() {
    const char* directory = captureDirectory();
    std::filesystem::remove_all(directory);
    auto* const block = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    for (std::size_t frame = 0; frame < Frames; ++frame) submitRewindFrame(block, frame, *output);
    AgcDriverWaitIdle_nid_postfix();
    check(std::filesystem::exists(directory), "flips made by rewind tails did not reach the frame recorder, so the capture did not start");
    checkCapturedFrames(directory);
}

void testRewindReleasedAfterCall() {
    const char* directory = captureDirectory();
    std::filesystem::remove_all(directory);
    auto* const block = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    std::chrono::steady_clock::duration slowest{};
    std::uint32_t* parked = nullptr;
    for (std::size_t frame = 0; frame < Frames; ++frame) {
        const auto began = std::chrono::steady_clock::now();
        auto* const control = submitRewound(block, frame);
        slowest = std::max(slowest, std::chrono::steady_clock::now() - began);
        if (parked != nullptr) {
            release(parked);
            waitPresented(*output, static_cast<int>(frame));
        }
        parked = control;
    }
    release(parked);
    waitPresented(*output, static_cast<int>(Frames));
    for (std::size_t frame = Frames; frame < 2 * Frames; ++frame) submitRewindFrame(block, frame, *output);
    AgcDriverWaitIdle_nid_postfix();
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(slowest).count();
    check(slowest < std::chrono::seconds(1), "a driver call waited " + std::to_string(waited) + " ms at a frame boundary for a REWIND its thread releases after the call");
    check(std::filesystem::exists(directory), "the capture did not start at an idle frame boundary after the busy ones");
    checkCapturedFrames(directory);
    const FrameCaptureReader reader{std::filesystem::path(directory)};
    const auto start = std::find_if(reader.Calls().begin(), reader.Calls().end(), [](const auto& call) { return call.kind == FrameCallKind::FrameStart; });
    check(start->values.at(0) >= Frames - 1, "the capture started at frame boundary " + std::to_string(start->values.at(0)) + ", while a REWIND was still parked");
}

void testFlipsCountPerThread() {
    auto* const block = allocate();
    const auto& driver = AgcDriver::DriverDetail::Driver::Get();
    const auto before = driver.FlipSubmissions();
    std::uint64_t otherBefore = 0, otherAfter = 0;
    std::exception_ptr failure;
    std::thread submitter([&] {
        try {
            otherBefore = driver.FlipSubmissions();
            submitEopFrame(block, 0);
            otherAfter = driver.FlipSubmissions();
        } catch (...) {
            failure = std::current_exception();
        }
    });
    submitter.join();
    if (failure) std::rethrow_exception(failure);
    AgcDriverWaitIdle_nid_postfix();
    check(otherAfter == otherBefore + 1, "the flipping submission did not count on the thread that made it");
    check(driver.FlipSubmissions() == before, "a flip submitted on another thread changed this thread's flip count, so a submission of this thread that overlaps it would end a captured frame");
}

void testSettledHoldsAdmissions() {
    AgcDriverWaitIdle_nid_postfix();
    check(AgcDriverHoldSettled_nid_postfix(1000), "the idle driver did not settle");
    std::uint64_t before = 0, completed = 0;
    AgcDriverSerialProgress_nid_postfix(&before, &completed);
    std::atomic<bool> admitted{false};
    std::thread submitter([&] {
        AgcDriverSuspendPoint_nid_postfix();
        admitted.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::uint64_t accepted = 0;
    AgcDriverSerialProgress_nid_postfix(&accepted, &completed);
    const bool held = !admitted.load() && accepted == before;
    AgcDriverReleaseSettled_nid_postfix();
    submitter.join();
    AgcDriverWaitIdle_nid_postfix();
    check(held, "the driver admitted a submission while a settled hold was taken on an idle driver");
}

void submitWords(std::span<const std::uint32_t> words, std::byte* at) {
    std::memcpy(at, words.data(), words.size_bytes());
    Packet packet{reinterpret_cast<std::uint32_t*>(at), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "a submission was refused");
}

void testWaitingRecorderAdmitsBesideBlockedFlip() {
    auto* const block = allocate();
    auto output = std::make_shared<HeldOutput>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    auto* const done = reinterpret_cast<std::uint32_t*>(block + BlockBytes / 2);
    auto* const registers = reinterpret_cast<std::uint32_t*>(block + BlockBytes / 2 + 256);
    registers[0] = 0x10;
    registers[1] = 74;
    const auto at = [](const void* pointer) { return reinterpret_cast<std::uintptr_t>(pointer); };
    const std::array<std::uint32_t, 10> listed{
        0xc0039f00u, static_cast<std::uint32_t>(at(registers)), static_cast<std::uint32_t>(at(registers) >> 32u), 0x80000000u, 1u,
        0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(at(done)), static_cast<std::uint32_t>(at(done) >> 32u), 1u};
    submitWords(listed, block);
    std::exception_ptr flipFailure;
    std::thread flipper([&] {
        try {
            const std::array<std::uint32_t, 6> flip{AgcDriver::FlipPacketHeader, Handle, 0, 1, 0, 0};
            submitWords(flip, block + 1024);
        } catch (...) {
            flipFailure = std::current_exception();
        }
    });
    const bool held = output->WaitUntilHeld();
    std::promise<void> admitted;
    auto admission = admitted.get_future();
    std::thread writer([&] {
        const std::array<std::uint32_t, 2> nop{0xc0001000u, 0};
        submitWords(nop, block + 2048);
        admitted.set_value();
    });
    const bool besideFlip = admission.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    output->Open();
    writer.join();
    flipper.join();
    if (flipFailure) std::rethrow_exception(flipFailure);
    AgcDriverWaitIdle_nid_postfix();
    check(held, "the flip submission did not wait for flip room");
    check(besideFlip, "with the frame recorder waiting for its trigger, a submission waited for another thread's submission that was waiting for flip room");
    check(std::atomic_ref<std::uint32_t>(*done).load() == 1, "the register list submitted before the flip was not executed");
    check(!std::filesystem::exists(captureDirectory()) || std::filesystem::is_empty(captureDirectory()), "the frame recorder started without its trigger");
}

class Latch {
public:
    void Open() {
        {
            std::lock_guard lock(mutex);
            open = true;
        }
        changed.notify_all();
    }
    bool Wait() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(10), [&] { return open; });
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    bool open = false;
};

void finishedWithin(const std::function<void()>& call, const char* what) {
    auto done = std::make_shared<std::promise<void>>();
    auto finished = done->get_future();
    std::thread([call, done] {
        try {
            call();
            done->set_value();
        } catch (...) {
            done->set_exception(std::current_exception());
        }
    }).detach();
    if (finished.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        std::fprintf(stderr, "FAILED: %s did not return within 10 s\n", what);
        std::fflush(stderr);
        std::_Exit(1);
    }
    finished.get();
}

void testRecorderStartsAfterAdmittedCalls() {
    auto* const recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "the frame recorder is not active");
    const std::filesystem::path directory(captureDirectory());
    const char* trigger = std::getenv("APS5_FRAME_CAPTURE_TRIGGER");
    check(trigger != nullptr && *trigger != '\0', "the startup test needs APS5_FRAME_CAPTURE_TRIGGER");
    std::filesystem::remove(trigger);
    std::filesystem::remove_all(directory);
    const auto started = [&] { return std::filesystem::exists(directory / FrameCaptureWriter::MemoryName); };
    std::atomic<bool> holding{false};
    std::atomic<std::uint64_t> waits{0};
    std::atomic<std::uint64_t> waitsWhileHeld{0};
    AgcDriver::FrameRecorder::SetStartIdleWaitForTest([&] {
        waits.fetch_add(1);
        if (holding.load()) waitsWhileHeld.fetch_add(1);
        return AgcDriver::DriverDetail::Driver::Get().WaitIdleUnlessTitleWaitsFor(std::chrono::seconds(10));
    });
    struct Restore {
        ~Restore() { AgcDriver::FrameRecorder::SetStartIdleWaitForTest({}); }
    } restore;
    auto* const block = allocate();
    const std::array<std::uint32_t, 2> nop{0xc0001000u, 0};
    std::memcpy(block, nop.data(), sizeof(nop));
    const Packet idle{reinterpret_cast<std::uint32_t*>(block), static_cast<std::uint32_t>(nop.size()), 0, {}};

    bool inner = false;
    finishedWithin([&] { recorder->Submit(idle, 0, [&] { recorder->Submit(idle, 0, [&] { inner = true; }); }); }, "a call accepted from inside another call's acceptance");
    check(inner, "the call accepted from inside another call's acceptance did not run");

    bool thrown = false;
    try {
        recorder->Submit(idle, 0, [] { throw std::runtime_error("refused by the test"); });
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    check(thrown, "a call whose acceptance threw did not pass the exception on");

    auto* const label = reinterpret_cast<std::uint32_t*>(block + BlockBytes / 2);
    const auto address = reinterpret_cast<std::uintptr_t>(label);
    const std::array<std::uint32_t, 5> write{0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1u};
    std::memcpy(block + 1024, write.data(), sizeof(write));
    const Packet labelled{reinterpret_cast<std::uint32_t*>(block + 1024), static_cast<std::uint32_t>(write.size()), 0, {}};
    Latch entered;
    Latch release;
    std::exception_ptr failure;
    std::thread admitted([&] {
        try {
            recorder->Submit(labelled, 0, [&] {
                holding.store(true);
                entered.Open();
                check(release.Wait(), "the test did not release the call held in its acceptance");
                check(sceAgcDriverSubmitDcb(const_cast<Packet*>(&labelled)) == 0, "the held call's submission was refused");
            });
        } catch (...) {
            failure = std::current_exception();
        }
    });
    check(entered.Wait(), "the held call did not reach its acceptance");
    { std::ofstream(trigger) << 1; }
    finishedWithin([&] { recorder->FrameBoundary(); }, "a frame boundary while a call was being accepted");
    const bool deferred = !started();
    holding.store(false);
    release.Open();
    admitted.join();
    if (failure) std::rethrow_exception(failure);
    check(deferred, "the frame recorder started while a call it does not record was still being accepted");
    check(waitsWhileHeld.load() == 0, "the frame recorder waited for the driver to go idle while a call it does not record was still being accepted");
    finishedWithin([&] { recorder->FrameBoundary(); }, "a frame boundary after the held call");
    check(started(), "the frame recorder did not start once no call was being accepted, so a call whose acceptance threw or re-entered is still counted");
    check(waits.load() == 1, "the frame recorder did not wait for the driver to go idle exactly once, at the start");
    finishedWithin([&] { recorder->FrameBoundary(); }, "the frame boundary that ends the capture");
    AgcDriverWaitIdle_nid_postfix();
    const FrameCaptureReader reader(directory);
    const bool snapshotted = std::ranges::any_of(reader.Chunks(), [&](const auto& chunk) {
        if (chunk.kind != FrameMemoryKind::Snapshot || chunk.address > address || address + 4 > chunk.address + chunk.bytes) return false;
        const auto bytes = reader.Read(chunk);
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data() + (address - chunk.address), sizeof(value));
        return value == 1;
    });
    check(snapshotted, "the snapshot does not hold the label written by the GPU work of a call accepted before the start");
}

}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "rewind") {
            testRewindTailFramesCount();
            std::puts("frame capture rewind tail tests passed");
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "startup") {
            testRecorderStartsAfterAdmittedCalls();
            std::puts("frame capture startup tests passed");
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "waiting") {
            testWaitingRecorderAdmitsBesideBlockedFlip();
            std::puts("frame capture waiting recorder tests passed");
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "rewind_held") {
            testRewindReleasedAfterCall();
            std::puts("frame capture busy boundary tests passed");
            return 0;
        }
        testEopFramesCountOnce();
        testFlipsCountPerThread();
        testSettledHoldsAdmissions();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
    std::puts("frame capture EOP tests passed");
    return 0;
}
