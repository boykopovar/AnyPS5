#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <limits>
#include <vector>

#undef main

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try { action(); }
    catch (const std::runtime_error& error) { return error.what(); }
    throw std::runtime_error("expected an exception");
}

class Gate final : public AgcDriver::IVideoOutput, public AgcDriver::IFlipRequest, public std::enable_shared_from_this<Gate> {
public:
    std::mutex mutex;
    std::condition_variable changed;
    bool released = false;
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override { return shared_from_this(); }
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; })) throw std::runtime_error("test gate timed out");
    }
    void Fail(std::exception_ptr error) noexcept override { if (!error) std::terminate(); }
    void Release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};

std::span<std::byte> alignedBuffer(std::vector<std::byte>& allocation) {
    const auto address = reinterpret_cast<uintptr_t>(allocation.data());
    const auto offset = (65536u - (address & 65535u)) & 65535u;
    return {allocation.data() + offset, allocation.size() - 65535u};
}

void testLifetime(bool reopen) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    auto gate = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, gate);
    std::array<uint32_t, 6> words{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet packet{words.data(), 6, 0, {}};
    sceAgcDriverSubmitDcb(&packet);
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    sceVideoOutSubmitFlip(handle, 0, 1, -9);
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->flipStatus.flipPendingNum == 1 && cfg->flipStatus.count == 0, "reservation status is wrong");
    }
    attribute.dcc_control = 1;
    check(expectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr); }).find("pending flip") != std::string::npos, "pending attributes were changed");
    check(expectFailure([&] { sceVideoOutUnregisterBuffers(handle, 0); }).find("pending flip") != std::string::npos, "pending buffer was unregistered");
    check(sceVideoOutWaitVblank(handle) == 0 && sceVideoOutWaitVblank(handle) == 0, "vblank stalled behind the GPU queue");
    std::shared_ptr<VideoOutConfig> replacement;
    sceVideoOutClose(handle);
    if (reopen) {
        check(sceVideoOutOpen(255, 0, 0, nullptr) == handle, "reopen changed handle");
        replacement = VideoOutDriver::Get().GetConfig(handle);
        check(replacement != cfg && replacement->generation > cfg->generation, "reopen reused old port state");
    }
    gate->Release();
    std::exception_ptr failure;
    {
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(10), [&] { return cfg->failure != nullptr; }), "flip failure did not wake waiters");
        failure = cfg->failure;
        check(cfg->flipStatus.count == 0 && cfg->flipStatus.flipPendingNum == 0, "failed flip has successful or pending status");
    }
    const auto message = expectFailure([&] { std::rethrow_exception(failure); });
    check(message.find("closed") != std::string::npos, "flip used a closed port");
    if (replacement) {
        std::lock_guard lock(replacement->mutex);
        check(replacement->flipStatus.flipPendingNum == 0 && replacement->flipStatus.count == 0, "old request changed new port counters");
    }
    check(expectFailure([&] { sceVideoOutWaitVblank(handle); }).find("closed") != std::string::npos, "VideoOut lost worker failure");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, gate);
    if (reopen) sceVideoOutClose(handle);
    const auto shutdown = expectFailure([] { LibcRunShutdown_nid_postfix(); });
    check(shutdown.find("closed") != std::string::npos, "shutdown lost asynchronous error");
}

std::size_t tiledOffset(uint32_t x, uint32_t y, uint32_t width) {
    constexpr std::array<uint32_t, 7> xMasks{4, 8, 128, 256, 0x2200, 0x800, 0x8400};
    constexpr std::array<uint32_t, 7> yMasks{16, 32, 64, 0x1100, 0x200, 0x400, 0x4800};
    uint32_t offset = 0;
    for (uint32_t bit = 0; bit < 7; ++bit) {
        if ((x & (1u << bit)) != 0) offset ^= xMasks[bit];
        if ((y & (1u << bit)) != 0) offset ^= yMasks[bit];
    }
    return (static_cast<std::size_t>(y / 128u) * ((width + 127u) / 128u) + x / 128u) * 65536u + offset;
}

void fillBuffer(std::span<std::byte> bytes, uint32_t width, uint32_t height) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const auto offset = tiledOffset(x, y, width);
            bytes[offset] = static_cast<std::byte>(x & 255u);
            bytes[offset + 1] = static_cast<std::byte>(y & 255u);
            bytes[offset + 2] = static_cast<std::byte>((x ^ y) & 255u);
            bytes[offset + 3] = std::byte{255};
        }
    }
}

void testDecode() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = alignedBuffer(allocation);
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<uint64_t>(tiled.data()), 0x8000000000000000ull, 259, 137};
    check(AgcDriver::DisplayBufferSize(buffer) == tiled.size(), "incorrect padded display footprint");
    fillBuffer(tiled, buffer.width, buffer.height);
    for (const auto format : {0x8000000000000000ull, 0x8000000022000000ull}) {
        buffer.pixelFormat = format;
        const auto pixels = AgcDriver::ReadDisplayBuffer(buffer);
        for (uint32_t y = 0; y < buffer.height; ++y) {
            for (uint32_t x = 0; x < buffer.width; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
                const auto blue = format == 0x8000000000000000ull ? x : x ^ y;
                const auto red = format == 0x8000000000000000ull ? x ^ y : x;
                check(pixels[offset] == static_cast<std::byte>(blue & 255u) && pixels[offset + 1] == static_cast<std::byte>(y & 255u) && pixels[offset + 2] == static_cast<std::byte>(red & 255u) && pixels[offset + 3] == std::byte{255}, "detiled pixel or channel order mismatch");
            }
        }
    }
    expectFailure([&] { AgcDriver::DecodeDisplayBuffer(buffer, std::span(tiled).first(tiled.size() - 1)); });
    buffer.pixelFormat = 0;
    expectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.pixelFormat = 0x8000000000000000ull;
    ++buffer.address;
    expectFailure([&] { AgcDriver::ReadDisplayBuffer(buffer); });
    buffer.address = std::numeric_limits<uint64_t>::max() & ~uint64_t{65535};
    expectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
}

void testControls() {
    const auto handle = sceVideoOutOpen(255, 0, 0, nullptr);
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    for (int rate = 0; rate <= 2; ++rate) {
        check(sceVideoOutSetFlipRate(handle, rate) == 0 && cfg->flipRate == rate, "flip rate was not applied");
    }
    expectFailure([&] { sceVideoOutSetFlipRate(handle, 3); });
    VideoOutColorSettings settings{};
    expectFailure([&] { sceVideoOutColorSettingsSetGamma(&settings, std::numeric_limits<float>::quiet_NaN()); });
    KernelEqueue queue = 0;
    check(sceKernelCreateEqueue(&queue, "VideoOut test") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(queue);
    check(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0, "output mode subscription failed");
    KernelEvent event{};
    check(owner->GetTriggeredEvents(&event, 1) == 1, "initial output mode event missing");
    int64_t mode = 0;
    check(sceVideoOutGetEventData(&event, &mode) == 0 && mode == VIDEO_OUT_OUTPUT_MODE_DEFAULT && sceVideoOutGetEventCount(&event) == 1, "initial output mode event encoding is wrong");
    check(sceVideoOutAddVblankEvent(queue, handle, nullptr) == 0 && sceVideoOutAddVblankEvent(queue, handle, &settings) == 0, "vblank subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->vblankEvents.size() == 1, "duplicate vblank subscription");
    }
    check(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    check(owner->GetTriggeredEvents(&event, 1) == 1 && event.udata == &settings && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VBLANK, "vblank event or updated user data missing");
    sceVideoOutClose(handle);
    check(owner->GetTriggeredEvents(&event, 1) == 0, "closed port retained pending events");
    check(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

void testPresentation(bool expectUnavailable) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    auto invalid = attribute;
    invalid.dcc_control = 1;
    check(expectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &invalid, 0, nullptr); }).find("DCC") != std::string::npos, "DCC buffer was accepted");
    check(!cfg->groups[0].occupied, "rejected registration changed the buffer group");
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    sceVideoOutSetFlipRate(handle, 2);
    for (int index : {0, -2, -1, 0}) {
        uint64_t target;
        uint64_t previousVblank;
        {
            std::lock_guard lock(cfg->mutex);
            target = cfg->flipStatus.count + 1;
            previousVblank = cfg->lastFlipVblank;
        }
        sceVideoOutSubmitFlip(handle, index, 1, -123456789);
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == target; }), "presentation did not complete");
        if (expectUnavailable) {
            check(cfg->failure != nullptr && cfg->flipStatus.count == 0 && cfg->bufferPending[0] == 0, "failed presentation was marked complete or retained its buffer");
            const auto error = cfg->failure;
            lock.unlock();
            const auto message = expectFailure([&] { std::rethrow_exception(error); });
            check(message.find("required instance extension missing") != std::string::npos, "unexpected presentation failure");
            check(expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }) == message, "AGC lost presentation failure");
            check(expectFailure([] { LibcRunShutdown_nid_postfix(); }) == message, "shutdown lost presentation failure");
            return;
        }
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(cfg->flipStatus.flipArg == -123456789 && cfg->flipStatus.currentBuffer == index && cfg->flipStatus.flipPendingNum == 0, "presentation status is wrong");
        check(cfg->lastFlipVblank >= previousVblank + 3, "flip rate did not wait for its interval");
        lock.unlock();
        attribute.width = 65;
        attribute.height = 33;
        sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr);
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

// Flips of two buffers submitted without waiting for each other: every one completes in order
// (presentations trail on the GPU, APS5_FLIP_INFLIGHT), and the shutdown retires the ones in flight.
void testBackToBack() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::array<std::vector<std::byte>, 2> allocations{std::vector<std::byte>(6 * 65536 + 65535), std::vector<std::byte>(6 * 65536 + 65535)};
    std::array<VideoOutBuffers, 2> buffers{};
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        const auto storage = alignedBuffer(allocations[i]);
        fillBuffer(storage, 259, 137);
        buffers[i] = {storage.data(), nullptr, {nullptr, nullptr}};
    }
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, buffers.data(), 2, &attribute, 0, nullptr);
    int64_t argument = 1000;
    for (int index : {0, 1, 0, 1}) sceVideoOutSubmitFlip(handle, index, 1, ++argument);
    std::unique_lock lock(cfg->mutex);
    check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == 4; }), "back-to-back presentations did not complete");
    if (cfg->failure) std::rethrow_exception(cfg->failure);
    check(cfg->flipStatus.flipPendingNum == 0 && cfg->bufferPending[0] == 0 && cfg->bufferPending[1] == 0, "back-to-back flips left pending counts");
    check(cfg->flipStatus.flipArg == argument && cfg->flipStatus.currentBuffer == 1, "back-to-back flip status is wrong");
    lock.unlock();
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "decode") testDecode();
        else if (argc == 2 && std::string(argv[1]) == "controls") testControls();
        else if (argc == 2 && std::string(argv[1]) == "present") testPresentation(false);
        else if (argc == 2 && std::string(argv[1]) == "backtoback") testBackToBack();
        else if (argc == 2 && std::string(argv[1]) == "unavailable") testPresentation(true);
        else testLifetime(argc == 2 && std::string(argv[1]) == "reopen");
        std::puts("VideoOut flip tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
