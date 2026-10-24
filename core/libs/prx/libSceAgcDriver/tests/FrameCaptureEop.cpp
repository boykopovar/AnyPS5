#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using AgcDriver::FrameCallKind;
using AgcDriver::FrameCaptureReader;

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

void testEopFramesCountOnce() {
    const char* directory = std::getenv("APS5_FRAME_CAPTURE");
    check(directory != nullptr && *directory != '\0', "APS5_FRAME_CAPTURE is not set");
    auto* const block = allocate();
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(Handle, output);
    for (std::size_t frame = 0; frame < Frames; ++frame) submitEopFrame(block, frame);
    AgcDriverWaitIdle_nid_postfix();
    check(output->ready.load() == static_cast<int>(Frames), "the driver presented " + std::to_string(output->ready.load()) + " of the EOP flips");

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

}

int main() {
    try {
        testEopFramesCountOnce();
        testFlipsCountPerThread();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
    std::puts("frame capture EOP tests passed");
    return 0;
}
