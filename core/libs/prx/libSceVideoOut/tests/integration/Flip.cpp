#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <system_error>
#include <limits>
#include <thread>
#include <vector>

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
extern "C" int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
extern "C" int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);

namespace {

using Testing::Require;

template<typename TAction>
std::string ExpectFailure(TAction action) {
    return Testing::RequireThrows<std::runtime_error>(action, "expected an exception").what();
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

std::span<std::byte> AlignedBuffer(std::vector<std::byte>& allocation) {
    const auto address = reinterpret_cast<uintptr_t>(allocation.data());
    const auto offset = (65536u - (address & 65535u)) & 65535u;
    return {allocation.data() + offset, allocation.size() - 65535u};
}

void RunLifetime(bool reopen) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    auto gate = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, gate);
    std::array<uint32_t, 6> words{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet packet{words.data(), 6, 0, {}};
    sceAgcDriverSubmitDcb(&packet);
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    sceVideoOutSubmitFlip(handle, 0, 1, -9);
    {
        std::lock_guard lock(cfg->mutex);
        Require(cfg->flipStatus.flipPendingNum == 1 && cfg->flipStatus.count == 0, "reservation status is wrong");
    }
    attribute.dcc_control = 1;
    Require(ExpectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr); }).find("pending flip") != std::string::npos, "pending attributes were changed");
    Require(ExpectFailure([&] { sceVideoOutUnregisterBuffers(handle, 0); }).find("pending flip") != std::string::npos, "pending buffer was unregistered");
    Require(sceVideoOutWaitVblank(handle) == 0 && sceVideoOutWaitVblank(handle) == 0, "vblank stalled behind the GPU queue");
    std::shared_ptr<VideoOutConfig> replacement;
    sceVideoOutClose(handle);
    if (reopen) {
        Require(sceVideoOutOpen(255, 0, 0, nullptr) == handle, "reopen changed handle");
        replacement = VideoOutDriver::Get().GetConfig(handle);
        Require(replacement != cfg && replacement->generation > cfg->generation, "reopen reused old port state");
    }
    gate->Release();
    std::exception_ptr failure;
    {
        std::unique_lock lock(cfg->mutex);
        Require(cfg->vblankCond.wait_for(lock, std::chrono::seconds(10), [&] { return cfg->failure != nullptr; }), "flip failure did not wake waiters");
        failure = cfg->failure;
        Require(cfg->flipStatus.count == 0 && cfg->flipStatus.flipPendingNum == 0, "failed flip has successful or pending status");
    }
    const auto message = ExpectFailure([&] { std::rethrow_exception(failure); });
    Require(message.find("closed") != std::string::npos, "flip used a closed port");
    if (replacement) {
        std::lock_guard lock(replacement->mutex);
        Require(replacement->flipStatus.flipPendingNum == 0 && replacement->flipStatus.count == 0, "old request changed new port counters");
    }
    Require(ExpectFailure([&] { sceVideoOutWaitVblank(handle); }).find("closed") != std::string::npos, "VideoOut lost worker failure");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, gate);
    if (reopen) sceVideoOutClose(handle);
    const auto shutdown = ExpectFailure([] { LibcRunShutdown_nid_postfix(); });
    Require(shutdown.find("closed") != std::string::npos, "shutdown lost asynchronous error");
}

std::size_t TiledOffset(uint32_t x, uint32_t y, uint32_t width) {
    constexpr std::array<uint32_t, 7> xMasks{4, 8, 128, 256, 0x2200, 0x800, 0x8400};
    constexpr std::array<uint32_t, 7> yMasks{16, 32, 64, 0x1100, 0x200, 0x400, 0x4800};
    uint32_t offset = 0;
    for (uint32_t bit = 0; bit < 7; ++bit) {
        if ((x & (1u << bit)) != 0) offset ^= xMasks[bit];
        if ((y & (1u << bit)) != 0) offset ^= yMasks[bit];
    }
    return (static_cast<std::size_t>(y / 128u) * ((width + 127u) / 128u) + x / 128u) * 65536u + offset;
}

void FillBuffer(std::span<std::byte> bytes, uint32_t width, uint32_t height) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const auto offset = TiledOffset(x, y, width);
            bytes[offset] = static_cast<std::byte>(x & 255u);
            bytes[offset + 1] = static_cast<std::byte>(y & 255u);
            bytes[offset + 2] = static_cast<std::byte>((x ^ y) & 255u);
            bytes[offset + 3] = std::byte{255};
        }
    }
}

void RunDecodeDetile() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = AlignedBuffer(allocation);
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<uint64_t>(tiled.data()), 0x8000000000000000ull, 259, 137};
    Require(AgcDriver::DisplayBufferSize(buffer) == tiled.size(), "incorrect padded display footprint");
    FillBuffer(tiled, buffer.width, buffer.height);
    for (const auto format : {0x8000000000000000ull, 0x8000000022000000ull}) {
        buffer.pixelFormat = format;
        const auto pixels = AgcDriver::ReadDisplayBuffer(buffer);
        for (uint32_t y = 0; y < buffer.height; ++y) {
            for (uint32_t x = 0; x < buffer.width; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * buffer.width + x) * 4;
                const auto blue = format == 0x8000000000000000ull ? x : x ^ y;
                const auto red = format == 0x8000000000000000ull ? x ^ y : x;
                Require(pixels[offset] == static_cast<std::byte>(blue & 255u) && pixels[offset + 1] == static_cast<std::byte>(y & 255u) && pixels[offset + 2] == static_cast<std::byte>(red & 255u) && pixels[offset + 3] == std::byte{255}, "detiled pixel or channel order mismatch");
            }
        }
    }
}

void RunDecodeInvalid() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = AlignedBuffer(allocation);
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<uint64_t>(tiled.data()), 0x8000000000000000ull, 259, 137};
    ExpectFailure([&] { AgcDriver::DecodeDisplayBuffer(buffer, std::span(tiled).first(tiled.size() - 1)); });
    buffer.pixelFormat = 0;
    ExpectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
    buffer.pixelFormat = 0x8000000000000000ull;
    ++buffer.address;
    ExpectFailure([&] { AgcDriver::ReadDisplayBuffer(buffer); });
    buffer.address = std::numeric_limits<uint64_t>::max() & ~uint64_t{65535};
    ExpectFailure([&] { AgcDriver::DisplayBufferSize(buffer); });
}

void RunDescribeOptions() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = AlignedBuffer(allocation);
    BufferAttributeGroup group{};
    const VideoOutBuffer registered{0, reinterpret_cast<uint64_t>(tiled.data()), 0};
    sceVideoOutSetBufferAttribute2(&group.attribute, 0x8000000000000000ull, 0, 259, 137, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, 0, 0);
    group.occupied = true;
    const auto plain = DescribeVideoOutBuffer(registered, group);
    group.attribute.option = VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY;
    const auto strict = DescribeVideoOutBuffer(registered, group);
    Require(strict.address == plain.address && strict.pixelFormat == plain.pixelFormat && strict.width == plain.width && strict.height == plain.height && strict.tilingMode == plain.tilingMode && strict.pitchInPixel == plain.pitchInPixel, "the STRICT_COLORIMETRY option changed the described buffer");
    for (const uint64_t option : {1ull, 32ull, 40ull}) {
        group.attribute.option = option;
        Require(ExpectFailure([&] { DescribeVideoOutBuffer(registered, group); }).find("buffer option " + std::to_string(option)) != std::string::npos, "an unsupported buffer option was accepted");
    }
}

void RunDescribeDcc() {
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto tiled = AlignedBuffer(allocation);
    BufferAttributeGroup group{};
    group.occupied = true;
    VideoOutBuffer dccBuffer{0, reinterpret_cast<uint64_t>(tiled.data()), 0x7f0000};
    sceVideoOutSetBufferAttribute2(&group.attribute, 0x8000000000000000ull, 0, 259, 137, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, 0x208, 0x11223344);
    group.category = VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED;
    const auto compressed = DescribeVideoOutBuffer(dccBuffer, group);
    Require(compressed.address == dccBuffer.dataAddress && compressed.dccAddress == 0x7f0000 && compressed.dccClearColor == 0x11223344, "DCC metadata or clear color was not described");
    group.attribute.dcc_control = 0x100024;
    Require(DescribeVideoOutBuffer(dccBuffer, group).dccAddress == 0x7f0000, "independent 128-byte DCC blocks were rejected");
    group.attribute.dcc_control = 0x209;
    Require(ExpectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("DCC control 0x209") != std::string::npos, "an unknown DCC control bit was accepted");
    group.attribute.dcc_control = 0x208;
    group.attribute.tiling_mode = 1;
    Require(ExpectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("linear") != std::string::npos, "a linear DCC buffer was accepted");
    group.attribute.tiling_mode = 0;
    dccBuffer.metadataAddress = 0;
    Require(ExpectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("without DCC metadata") != std::string::npos, "a compressed buffer without metadata was accepted");
    dccBuffer.metadataAddress = 0x7f0000;
    group.category = VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED;
    Require(ExpectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("DCC metadata 0x7f0000") != std::string::npos, "an uncompressed buffer with DCC metadata was accepted");
    group.category = 2;
    Require(ExpectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("category 2") != std::string::npos, "an unknown buffer category was accepted");
}

void RunOpenParam() {
    using Words = std::array<std::uint32_t, 6>;
    constexpr auto first = VIDEO_OUT_OPEN_PARAM_FIRST_WORD;
    for (const Words& rejected : {
             Words{24, 0, 0, 0, 0, 0},
             Words{first, 2, 0, 0, 0, 0},
             Words{first, 0, 0, 0x101, 0, 0},
             Words{first, 1, 255, 0, 0, 0},
             Words{first, 1, 768, 0, 0, 0},
             Words{first, 0, 0, 1, 0, 0},
             Words{first, 0, 0, 1, 0x2000, 0},
             Words{first, 0, 0, 1, 0, 1},
         }) {
        ExpectFailure([&] { sceVideoOutOpen(255, 0, 0, rejected.data()); });
    }
    std::array<std::uint32_t, 4> zeroed{first, 0, 0, 0};
    const auto zeroedHandle = sceVideoOutOpen(255, 0, 0, zeroed.data());
    Require(zeroedHandle >= 0, "open with a zeroed param failed");
    sceVideoOutClose(zeroedHandle);
    for (const Words& accepted : {Words{first, 1, 767, 0, 0, 0}, Words{first, 0, 0, 1, 1, 0}}) {
        const auto handle = sceVideoOutOpen(255, 0, 0, accepted.data());
        Require(handle >= 0, "open with a valid service thread setting failed");
        sceVideoOutClose(handle);
    }
}

void RunControls() {
    const std::array<std::uint32_t, 22> openParam{VIDEO_OUT_OPEN_PARAM_FIRST_WORD, 1, 0x100, 1, 0x1FFF};
    const auto handle = sceVideoOutOpen(255, 0, 0, openParam.data());
    Require(handle >= 0, "open with the highest priority on every CPU failed");
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    Require(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1, "default output mode is unsupported");
    Require(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE, "unavailable output mode is supported");
    Require(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE, "unavailable output mode was configured");
    Require(sceVideoOutIsOutputSupported(handle, 0xd000000aull, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE, "unknown output mode is supported");
    Require(sceVideoOutConfigureOutput(handle, 0xd000000aull, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE, "unknown output mode was configured");
    Require(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 0, "default output mode was rejected");
    for (int rate = 0; rate <= 2; ++rate) {
        Require(sceVideoOutSetFlipRate(handle, rate) == 0 && cfg->flipRate == rate, "flip rate was not applied");
    }
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 3); });
    VideoOutColorSettings settings{};
    ExpectFailure([&] { sceVideoOutColorSettingsSetGamma(&settings, std::numeric_limits<float>::quiet_NaN()); });
    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "VideoOut test") == 0, "event queue creation failed");
    auto owner = EqueuePin_nid_postfix(queue);
    Require(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0, "output mode subscription failed");
    KernelEvent event{};
    Require(owner->GetTriggeredEvents(&event, 1) == 1, "initial output mode event missing");
    int64_t mode = 0;
    Require(sceVideoOutGetEventData(&event, &mode) == 0 && mode == VIDEO_OUT_OUTPUT_MODE_DEFAULT && sceVideoOutGetEventCount(&event) == 1, "initial output mode event encoding is wrong");
    Require(sceVideoOutAddVblankEvent(queue, handle, nullptr) == 0 && sceVideoOutAddVblankEvent(queue, handle, &settings) == 0, "vblank subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        Require(cfg->vblankEvents.size() == 1, "duplicate vblank subscription");
    }
    Require(sceVideoOutAddVrrActiveStatusEvent(queue, handle, &settings) == 0, "VRR status subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        Require(cfg->vrrStatusEvents.size() == 1, "VRR status subscription was not recorded");
    }
    Require(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    Require(owner->GetTriggeredEvents(&event, 1) == 1 && event.udata == &settings && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VBLANK, "vblank event or updated user data missing");
    Require(sceVideoOutAddPreVblankStartEvent(queue, handle, &mode) == 0, "pre-vblank subscription failed");
    Require(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    KernelEvent events[2]{};
    Require(owner->GetTriggeredEvents(events, 2) == 2, "pre-vblank or vblank event missing");
    const auto& preVblank = sceVideoOutGetEventId(&events[0]) == VIDEO_OUT_EVENT_PRE_VBLANK_START ? events[0] : events[1];
    Require(sceVideoOutGetEventId(&preVblank) == VIDEO_OUT_EVENT_PRE_VBLANK_START && preVblank.udata == &mode, "pre-vblank event missing");
    Require(sceVideoOutDeletePreVblankStartEvent(queue, handle) == 0, "pre-vblank unsubscription failed");
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {allocation.data(), storage.data()}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "buffers with set reserved pointers were rejected");
    Require(sceVideoOutUnregisterBuffers(handle, 0) == 0, "buffer unregistration failed");
    Require(owner->GetTriggeredEvents(&event, 1) == 0, "VRR status event triggered without a VRR change");
    KernelEvent vrrStatus{};
    vrrStatus.ident = VIDEO_OUT_EVENT_VRR_STATUS;
    vrrStatus.filter = EVFILT_VIDEO_OUT;
    Require(sceVideoOutGetEventId(&vrrStatus) == VIDEO_OUT_EVENT_VRR_STATUS, "VRR status event id rejected");
    sceVideoOutClose(handle);
    Require(owner->GetTriggeredEvents(&event, 1) == 0, "closed port retained pending events");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

void RunPresentation() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    FillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    auto invalid = attribute;
    invalid.dcc_control = 1;
    Require(ExpectFailure([&] { sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &invalid, 0, nullptr); }).find("DCC") != std::string::npos, "DCC buffer was accepted");
    Require(!cfg->groups[0].occupied, "rejected registration changed the buffer group");
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
        Require(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == target; }), "presentation did not complete");
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(cfg->flipStatus.flipArg == -123456789 && cfg->flipStatus.currentBuffer == index && cfg->flipStatus.flipPendingNum == 0, "presentation status is wrong");
        Require(cfg->lastFlipVblank >= previousVblank + 3, "flip rate did not wait for its interval");
        lock.unlock();
        attribute.width = 65;
        attribute.height = 33;
        sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr);
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void RunUnavailable() {
    const auto message = ExpectFailure([] { sceVideoOutOpen(255, 0, 0, nullptr); });
    Require(ExpectFailure([] { sceVideoOutOpen(255, 0, 0, nullptr); }) == message, "a second open lost the presentation failure");
    std::printf("presentation unavailable: %s\n", message.c_str());
    LibcRunShutdown_nid_postfix();
}

void RunCompressedPresentation() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    FillBuffer(storage, 259, 137);
    std::vector<std::uint8_t> keys(4096);
    VideoOutBuffers buffer{storage.data(), keys.data(), {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0x208, 0xff102030);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED, nullptr);
    int64_t argument = 2000;
    for (std::uint8_t key : {0x00, 0xff, 0x20, 0xc0}) {
        std::fill(keys.begin(), keys.end(), key);
        uint64_t target;
        {
            std::lock_guard lock(cfg->mutex);
            target = cfg->flipStatus.count + 1;
        }
        sceVideoOutSubmitFlip(handle, 0, 1, ++argument);
        std::unique_lock lock(cfg->mutex);
        Require(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == target; }), "DCC presentation did not complete");
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(cfg->flipStatus.flipArg == argument && cfg->flipStatus.flipPendingNum == 0, "DCC flip status is wrong");
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void RunBackToBack() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::array<std::vector<std::byte>, 2> allocations{std::vector<std::byte>(6 * 65536 + 65535), std::vector<std::byte>(6 * 65536 + 65535)};
    std::array<VideoOutBuffers, 2> buffers{};
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        const auto storage = AlignedBuffer(allocations[i]);
        FillBuffer(storage, 259, 137);
        buffers[i] = {storage.data(), nullptr, {nullptr, nullptr}};
    }
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, buffers.data(), 2, &attribute, 0, nullptr);
    int64_t argument = 1000;
    for (int index : {0, 1, 0, 1}) sceVideoOutSubmitFlip(handle, index, 1, ++argument);
    std::unique_lock lock(cfg->mutex);
    Require(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == 4; }), "back-to-back presentations did not complete");
    if (cfg->failure) std::rethrow_exception(cfg->failure);
    Require(cfg->flipStatus.flipPendingNum == 0 && cfg->bufferPending[0] == 0 && cfg->bufferPending[1] == 0, "back-to-back flips left pending counts");
    Require(cfg->flipStatus.flipArg == argument && cfg->flipStatus.currentBuffer == 1, "back-to-back flip status is wrong");
    lock.unlock();
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

std::uint32_t BusyLoopResult(std::uint32_t iterations) {
    std::uint32_t value = 1;
    for (std::uint32_t i = 0; i < iterations; ++i) value = value * value + 1u;
    return value;
}

struct BusyLoop {
    volatile std::uint32_t* result;
    volatile std::uint32_t* label;
    std::vector<std::uint32_t> commands;
};

BusyLoop PrepareBusyLoop(std::uint32_t iterations) {
    alignas(256) static const std::array<std::uint32_t, 12> code{
        0xbe850380, 0x7e040281, 0xd5690002, 0x00020502, 0x4a040481, 0x80058105,
        0xbf070405, 0xbf85fffa, 0x7e020280, 0xe0701000, 0x80000201, 0xbf810000};
    static Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Shader);
    shader.shader_size = sizeof(code);
    shader.code = code.data();
    AgcDriverRegisterShader_nid_postfix(&shader);
    std::int64_t physical = 0;
    void* mapped = nullptr;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, 65536, 65536, 0, &physical) == 0, "direct memory allocation failed");
    Require(sceKernelMapDirectMemory(&mapped, 65536, 0x33, 0, physical, 65536) == 0, "direct memory mapping failed");
    auto* result = &static_cast<volatile std::uint32_t*>(mapped)[0];
    auto* label = &static_cast<volatile std::uint32_t*>(mapped)[64];
    *result = 0;
    *label = 0;
    const auto program = reinterpret_cast<std::uintptr_t>(code.data());
    const auto output = reinterpret_cast<std::uintptr_t>(result);
    const auto target = reinterpret_cast<std::uintptr_t>(label);
    return {result, label, {
        0xc0027600, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600, 0x213, 5u << 1u,
        0xc0037600, 0x207, 32, 1, 1,
        0xc0057600, 0x240, static_cast<std::uint32_t>(output), static_cast<std::uint32_t>(output >> 32u) & 0xffffu, 4, 0x31016fac, iterations,
        0xc0031500, 1, 1, 1, 0x8041,
        0xc0064900, 0x514, 1u << 29u, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(target >> 32u), 1, 0, 0}};
}

void RunReleaseVblank() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    FillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0, "flip rate was not applied");
    auto busy = PrepareBusyLoop(100000000u);
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    sceVideoOutSubmitFlip(handle, 0, 1, 1);
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(done, "the flip did not complete");
        Require(cfg->lastFlipVblank >= 3 && cfg->vblankStatus.count >= cfg->lastFlipVblank + 3, "the next flip is not paced from the vblank this flip was released at");
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void RunFlipAfterGpuWork() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    FillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    constexpr std::uint32_t iterations = 100000000u;
    auto busy = PrepareBusyLoop(iterations);
    auto& result = *busy.result;
    auto& label = *busy.label;
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    Require(sceVideoOutSubmitFlip(handle, 0, 1, 4) == 0, "the first flip was not accepted");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(done, "the first flip did not complete");
    }
    std::atomic<std::int64_t> landed{0};
    const auto start = std::chrono::steady_clock::now();
    const auto since = [&] { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count(); };
    std::thread poller([&] {
        while (label != 1 && since() < 30000000) std::this_thread::yield();
        if (label == 1) landed.store(since());
    });
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    Require(sceVideoOutSubmitFlip(handle, 0, 1, 5) == 0, "the flip was not accepted");
    std::int64_t flipped = 0;
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 2; });
        flipped = since();
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(done, "the flip did not complete");
    }
    poller.join();
    AgcDriverWaitIdle_nid_postfix();
    std::printf("GPU work landed at %.1f ms, flip completed at %.1f ms\n", landed.load() / 1000.0, flipped / 1000.0);
    Require(landed.load() >= 100000, "the GPU work did not land or was too short to order against the flip");
    Require(result == BusyLoopResult(iterations), "the GPU work stored the wrong result");
    Require(flipped + 20000 >= landed.load(), "the flip completed before the GPU work submitted ahead of it");
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void RunOneDevice() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = AlignedBuffer(allocation);
    FillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    constexpr std::uint32_t iterations = 1000u;
    auto busy = PrepareBusyLoop(iterations);
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (*busy.label != 1 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(*busy.label == 1 && *busy.result == BusyLoopResult(iterations), "the GPU work did not run");
    Require(sceVideoOutSubmitFlip(handle, 0, 1, 1) == 0, "the flip was not accepted");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        Require(done, "the flip did not complete");
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
    const AgcDriver::VulkanDevice next;
    Require(next.Serial() == 2, "the GPU work and the flip used more than one device");
}

class AppDirectory {
public:
    AppDirectory() : created(!std::filesystem::exists("app0")) {
        std::filesystem::create_directories("app0/sce_sys");
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0})";
        Require(static_cast<bool>(param), "write app0/sce_sys/param.json");
    }

    ~AppDirectory() {
        if (!created) return;
        std::error_code ignored;
        std::filesystem::remove_all("app0", ignored);
    }

    AppDirectory(const AppDirectory&) = delete;
    AppDirectory& operator=(const AppDirectory&) = delete;

private:
    const bool created;
};

void ShutdownAfterFailure() {
    try {
        LibcRunShutdown_nid_postfix();
    } catch (const std::exception& shutdown) {
        std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
    }
}

template<typename TBody>
void RunMode(const std::string& mode, TBody body) {
    const auto& arguments = Testing::Arguments();
    if (std::find(arguments.begin(), arguments.end(), mode) == arguments.end()) {
        Testing::Skip("needs its own process, runs only when the " + mode + " argument is given");
    }
    const AppDirectory app;
    try {
        body();
    } catch (const Testing::Failure&) {
        ShutdownAfterFailure();
        throw;
    } catch (const std::exception& error) {
        ShutdownAfterFailure();
        const std::string message = error.what();
        if (message.find("Vulkan support") != std::string::npos && !std::getenv("ANYPS5_REQUIRE_DISPLAY")) {
            Testing::Skip("no display or Vulkan device: " + message);
        }
        throw;
    }
}

const Testing::Case lifetime{"Flip_PortClosedWithPendingFlip_FailsFlipAndReportsAtShutdown", [] {
    RunMode("lifetime", [] { RunLifetime(false); });
}};

const Testing::Case reopen{"Flip_PortReopenedWithPendingFlip_KeepsNewPortStateClean", [] {
    RunMode("reopen", [] { RunLifetime(true); });
}};

const Testing::Case decodeDetile{"ReadDisplayBuffer_TiledBuffer_DetilesPixelsAndChannelOrder", [] {
    RunMode("decode", [] { RunDecodeDetile(); });
}};

const Testing::Case decodeInvalid{"DisplayBuffer_InvalidSizeFormatOrAddress_Throws", [] {
    RunMode("decode", [] { RunDecodeInvalid(); });
}};

const Testing::Case describeOptions{"DescribeVideoOutBuffer_BufferOptions_AcceptsStrictColorimetryOnly", [] {
    RunMode("decode", [] { RunDescribeOptions(); });
}};

const Testing::Case describeDcc{"DescribeVideoOutBuffer_DccSettings_DescribesOrRejects", [] {
    RunMode("decode", [] { RunDescribeDcc(); });
}};

const Testing::Case controls{"Controls_OpenParamsOutputModesFlipRateAndEvents_Behave", [] {
    RunMode("controls", [] {
        RunOpenParam();
        RunControls();
    });
}};

const Testing::Case presentation{"Flip_RegisteredBuffer_PresentsAtFlipRate", [] {
    RunMode("present", [] { RunPresentation(); });
}};

const Testing::Case backToBack{"Flip_BackToBackWithoutWaiting_CompletesInOrder", [] {
    RunMode("backtoback", [] { RunBackToBack(); });
}};

const Testing::Case compressed{"Flip_CompressedBuffer_PresentsEveryKey", [] {
    RunMode("dcc", [] { RunCompressedPresentation(); });
}};

const Testing::Case pacing{"Flip_AfterLongGpuWork_PacesFromReleaseVblank", [] {
    RunMode("pacing", [] { RunReleaseVblank(); });
}};

const Testing::Case afterGpu{"Flip_SubmittedAfterGpuWork_CompletesAfterIt", [] {
    RunMode("aftergpu", [] { RunFlipAfterGpuWork(); });
}};

const Testing::Case unavailable{"Open_PresentationUnavailable_ReportsSameFailureTwice", [] {
    RunMode("unavailable", [] { RunUnavailable(); });
}};

const Testing::Case oneDevice{"Flip_GpuWorkAndFlip_ShareOneDevice", [] {
    RunMode("onedevice", [] { RunOneDevice(); });
}};

} // namespace
