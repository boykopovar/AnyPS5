#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#undef main

extern "C" int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
extern "C" int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
extern "C" int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
extern "C" int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);

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
    auto smaller = attribute;
    smaller.width = 32;
    smaller.height = 32;
    check(sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &smaller, nullptr) == 0, "attributes of a buffer with a pending flip could not be changed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->groups[0].attribute.width == 32 && cfg->groups[0].attribute.height == 32 && cfg->flipStatus.flipPendingNum == 1, "the attribute change did not reach later flips or dropped the pending one");
    }
    attribute.dcc_control = 1;
    check(expectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute, nullptr); }).find("DCC") != std::string::npos, "an invalid attribute change was accepted");
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
    {
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(10), [&] { return cfg->flipStatus.flipPendingNum == 0; }), "closed port kept its pending flip");
        check(cfg->failure == nullptr, "closing a port failed the driver");
        check(cfg->flipStatus.count == 0, "cancelled flip has successful status");
    }
    if (replacement) {
        std::lock_guard lock(replacement->mutex);
        check(replacement->flipStatus.flipPendingNum == 0 && replacement->flipStatus.count == 0, "old request changed new port counters");
    } else {
        check(expectFailure([&] { sceVideoOutWaitVblank(handle); }).find("closed") != std::string::npos, "closed port accepted a vblank wait");
    }
    AgcDriverUnregisterVideoOutput_nid_postfix(7, gate);
    if (reopen) sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void testPs4Buffers() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    std::vector<std::byte> allocation(2 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBufferAttribute attribute{};
    sceVideoOutSetBufferAttribute(&attribute, 0x80000000u, 1, 0, 64, 64, 64);
    check(attribute.pixel_format == 0x80000000u && attribute.tiling_mode == 1 && attribute.aspect_ratio == 0 && attribute.width == 64 && attribute.height == 64 && attribute.pitch_in_pixel == 64 && attribute.option == 0 && attribute.reserved0 == 0 && attribute.reserved1 == 0, "PS4 buffer attribute fields are wrong");
    void* const pair[2] = {storage.data(), storage.data() + 20480};
    VideoOutBuffers second{pair[1], nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 ps5{};
    ps5.width = 64;
    ps5.height = 64;
    ps5.tiling_mode = 1;
    ps5.pitch_in_pixel = 64;
    ps5.pixel_format = 0x8000000000000000ull;
    check(expectFailure([&] { sceVideoOutRegisterBuffers2(handle, 3, 5, &second, 1, &ps5, 0, nullptr); }).find("64 KiB alignment") != std::string::npos, "a PS5 buffer was accepted at 4 KiB alignment");
    check(sceVideoOutRegisterBuffers(handle, 0, pair, 2, &attribute) == 0, "the first PS4 set index is not 0");
    void* const third[1] = {storage.data() + 49152};
    check(sceVideoOutRegisterBuffers(handle, 2, third, 1, &attribute) == 1, "the second PS4 set index is not 1");
    {
        const auto cfg = VideoOutDriver::Get().GetConfig(handle);
        std::lock_guard lock(cfg->mutex);
        check(cfg->groups[0].attribute.pixel_format == 0x8000000000000000ull && cfg->buffers[1].dataAddress == reinterpret_cast<uint64_t>(pair[1]) && cfg->buffers[2].groupIndex == 1, "PS4 buffers were registered wrongly");
    }
    VideoOutBufferAttribute2 tiledChange{};
    tiledChange.width = 64;
    tiledChange.height = 64;
    tiledChange.pixel_format = 0x8000000000000000ull;
    check(expectFailure([&] { sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &tiledChange, nullptr); }).find("only supported for linear buffers") != std::string::npos, "a PS4 buffer set was changed to tiled");
    void* const odd[1] = {storage.data() + 65536 + 256};
    check(expectFailure([&] { sceVideoOutRegisterBuffers(handle, 3, odd, 1, &attribute); }).find("4 KiB alignment") != std::string::npos, "a PS4 buffer was accepted below 4 KiB alignment");
    void* const fourth[1] = {storage.data() + 65536};
    auto tiled = attribute;
    tiled.tiling_mode = 0;
    tiled.pitch_in_pixel = 0;
    check(expectFailure([&] { sceVideoOutRegisterBuffers(handle, 3, fourth, 1, &tiled); }).find("tiled display buffers are not implemented") != std::string::npos, "a PS4 tiled buffer was accepted");
    auto tenBit = attribute;
    tenBit.pixel_format = 0x88060000u;
    check(expectFailure([&] { sceVideoOutRegisterBuffers(handle, 3, fourth, 1, &tenBit); }).find("unsupported PS4 pixel format 0x88060000") != std::string::npos, "an unknown PS4 pixel format was accepted");
    auto withOption = attribute;
    withOption.option = 1;
    check(expectFailure([&] { sceVideoOutRegisterBuffers(handle, 3, fourth, 1, &withOption); }).find("VIDEO_OUT_ERROR_INVALID_OPTION") != std::string::npos, "a PS4 buffer option was accepted");
    std::array<std::uint8_t, sizeof(VideoOutFlipStatus)> raw{};
    raw.fill(0xaa);
    check(sceVideoOutGetFlipStatus(handle, reinterpret_cast<VideoOutFlipStatus*>(raw.data())) == 0, "the PS4 flip status failed");
    check(std::all_of(raw.begin() + sizeof(VideoOutFlipStatusPs4), raw.end(), [](std::uint8_t value) { return value == 0xaa; }), "the PS4 flip status overflowed its 64 bytes");
    VideoOutFlipStatusPs4 flip;
    std::memcpy(&flip, raw.data(), sizeof(flip));
    check(flip.count == 0 && flip.flipArg == -1 && flip.currentBuffer == -1 && flip.flipPendingNum == 0, "the PS4 flip status fields are wrong");
    raw.fill(0xaa);
    check(sceVideoOutGetVblankStatus(handle, reinterpret_cast<VideoOutVblankStatus*>(raw.data())) == 0, "the PS4 vblank status failed");
    check(std::all_of(raw.begin() + sizeof(VideoOutVblankStatusPs4), raw.end(), [](std::uint8_t value) { return value == 0xaa; }), "the PS4 vblank status overflowed its 32 bytes");
    {
        const auto cfg = VideoOutDriver::Get().GetConfig(handle);
        check(sceVideoOutSubmitFlip(handle, 1, 1, 7) == 0, "the PS4 flip was not accepted");
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == 1; }), "the PS4 flip did not complete");
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(cfg->flipStatus.flipArg == 7 && cfg->flipStatus.currentBuffer == 1, "the PS4 flip status is wrong");
    }
    sceVideoOutClose(handle);
    const int ps5Handle = sceVideoOutOpen(255, 0, 0, nullptr);
    raw.fill(0xaa);
    check(sceVideoOutGetFlipStatus(ps5Handle, reinterpret_cast<VideoOutFlipStatus*>(raw.data())) == 0, "the PS5 flip status failed");
    check(std::all_of(raw.begin() + sizeof(VideoOutFlipStatusPs4), raw.end(), [](std::uint8_t value) { return value == 0; }), "the PS5 flip status did not fill its 128 bytes");
    sceVideoOutClose(ps5Handle);
    LibcRunShutdown_nid_postfix();
}

void testDeferredBuffer() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    auto gate = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, gate);
    std::array<uint32_t, 6> held{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet heldPacket{held.data(), 6, 0, {}};
    check(sceAgcDriverSubmitDcb(&heldPacket) == 0, "the gated flip was not accepted");
    std::array<uint32_t, 6> flip{0xc004105c, static_cast<uint32_t>(handle), 0, 1, 3, 0};
    Packet flipPacket{flip.data(), 6, 0, {}};
    check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "a GPU flip of a buffer registered before the GPU reaches it was rejected at submit");
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "buffer registration failed");
    gate->Release();
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the deferred flip did not complete");
        check(cfg->flipStatus.flipArg == 3 && cfg->flipStatus.currentBuffer == 0, "the deferred flip presented the wrong request");
    }
    AgcDriverWaitIdle_nid_postfix();
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    AgcDriverUnregisterVideoOutput_nid_postfix(7, gate);
    LibcRunShutdown_nid_postfix();
}

void testCpuFlipOrder() {
    const bool inOrder = std::getenv("APS5_CPU_FLIP_IN_ORDER") != nullptr;
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "buffer registration failed");
    const auto flipsWithin = [&](int count, std::chrono::milliseconds limit) {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, limit, [&] { return cfg->failure || cfg->flipStatus.count == count; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        return done;
    };
    const auto shownArgument = [&] {
        std::lock_guard lock(cfg->mutex);
        return cfg->flipStatus.flipArg;
    };
    auto first = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, first);
    std::array<uint32_t, 6> firstHeld{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet firstPacket{firstHeld.data(), 6, 0, {}};
    check(sceAgcDriverSubmitDcb(&firstPacket) == 0, "the first gated submission was not accepted");
    std::thread flipThread([&] { check(sceVideoOutSubmitFlip(handle, 0, 1, 5) == 0, "the flip thread's flip was not accepted"); });
    flipThread.join();
    const bool ahead = flipsWithin(1, std::chrono::milliseconds(inOrder ? 1000 : 8000));
    if (inOrder) check(!ahead, "with APS5_CPU_FLIP_IN_ORDER a flip completed while a queue-0 submission ahead of it was held");
    else check(ahead, "a flip from a thread that never submitted queue-0 work waited for a held queue-0 submission");
    first->Release();
    check(flipsWithin(1, std::chrono::seconds(30)), "the flip thread's flip did not complete");
    check(shownArgument() == 5, "the flip thread's flip presented the wrong request");
    auto second = std::make_shared<Gate>();
    AgcDriverRegisterVideoOutput_nid_postfix(8, second);
    std::array<uint32_t, 6> secondHeld{0xc004105c, 8, 0xfffffffeu, 1, 0, 0};
    Packet secondPacket{secondHeld.data(), 6, 0, {}};
    check(sceAgcDriverSubmitDcb(&secondPacket) == 0, "the second gated submission was not accepted");
    check(sceVideoOutSubmitFlip(handle, 0, 1, 6) == 0, "the submitting thread's flip was not accepted");
    check(!flipsWithin(2, std::chrono::milliseconds(1000)), "a flip from the thread that submitted queue-0 work completed ahead of that work");
    second->Release();
    check(flipsWithin(2, std::chrono::seconds(30)), "the submitting thread's flip did not complete");
    check(shownArgument() == 6, "the submitting thread's flip presented the wrong request");
    AgcDriverWaitIdle_nid_postfix();
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    AgcDriverUnregisterVideoOutput_nid_postfix(7, first);
    AgcDriverUnregisterVideoOutput_nid_postfix(8, second);
    LibcRunShutdown_nid_postfix();
}

void testUnregisteredBuffer() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "buffer registration failed");
    check(sceVideoOutUnregisterBuffers(handle, 0) == 0, "buffer unregistration failed");
    std::array<uint32_t, 6> flip{0xc004105c, static_cast<uint32_t>(handle), 0, 1, 7, 0};
    Packet flipPacket{flip.data(), 6, 0, {}};
    check(sceAgcDriverSubmitDcb(&flipPacket) == 0, "a GPU flip of an unregistered buffer was rejected at submit");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "a GPU flip of an unregistered buffer did not complete");
        check(cfg->flipStatus.flipPendingNum == 0 && cfg->flipStatus.flipArg == 7 && cfg->bufferPending[0] == 0, "a GPU flip of an unregistered buffer left pending state");
    }
    AgcDriverWaitIdle_nid_postfix();
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
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
    const VideoOutBuffer registered{0, reinterpret_cast<uint64_t>(tiled.data()), 0};
    BufferAttributeGroup group{};
    sceVideoOutSetBufferAttribute2(&group.attribute, 0x8000000000000000ull, 0, 259, 137, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, 0, 0);
    group.occupied = true;
    const auto plain = DescribeVideoOutBuffer(registered, group);
    group.attribute.option = VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY;
    const auto strict = DescribeVideoOutBuffer(registered, group);
    check(strict.address == plain.address && strict.pixelFormat == plain.pixelFormat && strict.width == plain.width && strict.height == plain.height && strict.tilingMode == plain.tilingMode && strict.pitchInPixel == plain.pitchInPixel, "the STRICT_COLORIMETRY option changed the described buffer");
    for (const uint64_t option : {1ull, 32ull, 40ull}) {
        group.attribute.option = option;
        check(expectFailure([&] { DescribeVideoOutBuffer(registered, group); }).find("buffer option " + std::to_string(option)) != std::string::npos, "an unsupported buffer option was accepted");
    }
    sceVideoOutSetBufferAttribute2(&group.attribute, 0x8000000000000000ull, 0, 259, 137, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, 0, 0);
    sceVideoOutAddBufferAttributeOption(&group.attribute, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY);
    check(group.attribute.option == VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY && group.attribute.width == 259 && group.attribute.height == 137, "an added buffer option was not stored");
    sceVideoOutAddBufferAttributeOption(&group.attribute, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY);
    check(group.attribute.option == VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY, "adding a buffer option twice changed it");
    check(DescribeVideoOutBuffer(registered, group).address == plain.address, "a buffer with an added supported option was rejected");
    sceVideoOutAddBufferAttributeOption(&group.attribute, 0x20);
    check(group.attribute.option == 0x28, "an added buffer option did not keep the existing bits");
    check(expectFailure([&] { DescribeVideoOutBuffer(registered, group); }).find("buffer option 40") != std::string::npos, "an unsupported added buffer option was accepted");
    check(expectFailure([&] { sceVideoOutAddBufferAttributeOption(nullptr, 0x20); }).find("null attribute") != std::string::npos, "a null attribute was accepted");
    sceVideoOutRemoveBufferAttributeOption(&group.attribute, 0x21);
    check(group.attribute.option == VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY && group.attribute.width == 259 && group.attribute.height == 137, "a removed buffer option was not cleared alone");
    check(DescribeVideoOutBuffer(registered, group).address == plain.address, "a buffer whose unsupported option was removed was rejected");
    sceVideoOutRemoveBufferAttributeOption(&group.attribute, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_STRICT_COLORIMETRY);
    check(group.attribute.option == VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, "removing the last buffer option did not clear it");
    check(expectFailure([&] { sceVideoOutRemoveBufferAttributeOption(nullptr, 0x20); }).find("null attribute") != std::string::npos, "a null attribute was accepted for removal");
    VideoOutBuffer dccBuffer{0, reinterpret_cast<uint64_t>(tiled.data()), 0x7f0000};
    sceVideoOutSetBufferAttribute2(&group.attribute, 0x8000000000000000ull, 0, 259, 137, VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE, 0x208, 0x11223344);
    group.category = VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED;
    const auto compressed = DescribeVideoOutBuffer(dccBuffer, group);
    check(compressed.address == dccBuffer.dataAddress && compressed.dccAddress == 0x7f0000 && compressed.dccClearColor == 0x11223344, "DCC metadata or clear color was not described");
    group.attribute.dcc_control = 0x100024;
    check(DescribeVideoOutBuffer(dccBuffer, group).dccAddress == 0x7f0000, "independent 128-byte DCC blocks were rejected");
    group.attribute.dcc_control = 0x209;
    check(expectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("DCC control 0x209") != std::string::npos, "an unknown DCC control bit was accepted");
    group.attribute.dcc_control = 0x208;
    group.attribute.tiling_mode = 1;
    check(expectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("linear") != std::string::npos, "a linear DCC buffer was accepted");
    group.attribute.tiling_mode = 0;
    dccBuffer.metadataAddress = 0;
    check(expectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("without DCC metadata") != std::string::npos, "a compressed buffer without metadata was accepted");
    dccBuffer.metadataAddress = 0x7f0000;
    group.category = VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED;
    check(expectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("DCC metadata 0x7f0000") != std::string::npos, "an uncompressed buffer with DCC metadata was accepted");
    group.category = 2;
    check(expectFailure([&] { DescribeVideoOutBuffer(dccBuffer, group); }).find("category 2") != std::string::npos, "an unknown buffer category was accepted");
}

void testOpenParam() {
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
        expectFailure([&] { sceVideoOutOpen(255, 0, 0, rejected.data()); });
    }
    std::array<std::uint32_t, 4> zeroed{first, 0, 0, 0};
    const auto zeroedHandle = sceVideoOutOpen(255, 0, 0, zeroed.data());
    check(zeroedHandle >= 0, "open with a zeroed param failed");
    sceVideoOutClose(zeroedHandle);
    for (const Words& accepted : {Words{first, 1, 767, 0, 0, 0}, Words{first, 0, 0, 1, 1, 0}}) {
        const auto handle = sceVideoOutOpen(255, 0, 0, accepted.data());
        check(handle >= 0, "open with a valid service thread setting failed");
        sceVideoOutClose(handle);
    }
}

void testControls() {
    testOpenParam();
    const std::array<std::uint32_t, 22> openParam{VIDEO_OUT_OPEN_PARAM_FIRST_WORD, 1, 0x100, 1, 0x1FFF};
    const auto handle = sceVideoOutOpen(255, 0, 0, openParam.data());
    check(handle >= 0, "open with the highest priority on every CPU failed");
    const auto cfg = VideoOutDriver::Get().GetConfig(handle);
    check(sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 1, "default output mode is unsupported");
    const int highFrameRate = sceVideoOutIsOutputSupported(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0);
    check(highFrameRate == 1 || highFrameRate == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE, "unexpected high-frame-rate support result");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_119_88HZ, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE, "unavailable output mode was configured");
    check(sceVideoOutIsOutputSupported(handle, 0xd000000aull, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE, "unknown output mode is supported");
    check(sceVideoOutConfigureOutput(handle, 0xd000000aull, nullptr, nullptr, 0) == VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE, "unknown output mode was configured");
    check(sceVideoOutConfigureOutput(handle, VIDEO_OUT_OUTPUT_MODE_DEFAULT, nullptr, nullptr, 0) == 0, "default output mode was rejected");
    for (int rate = 0; rate <= 2; ++rate) {
        check(sceVideoOutSetFlipRate(handle, rate) == 0 && cfg->flipRate == rate, "flip rate was not applied");
    }
    for (const int rate : {3, 60, -1}) {
        check(sceVideoOutSetFlipRate(handle, rate) == VIDEO_OUT_ERROR_INVALID_VALUE, "flip rate outside 0..2 was accepted");
        check(cfg->flipRate == 2, "a rejected flip rate changed the stored rate");
    }
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
    check(sceVideoOutDeleteOutputModeEvent(queue, handle) == 0, "output mode unsubscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->outputModeEvents.empty(), "output mode unsubscription did not clear registration");
    }
    check(sceVideoOutDeleteOutputModeEvent(queue, handle) == 0, "duplicate output mode unsubscription failed");
    expectFailure([&] { sceVideoOutDeleteOutputModeEvent(0, handle); });
    expectFailure([&] { sceVideoOutDeleteOutputModeEvent(queue, -1); });
    check(sceVideoOutAddVblankEvent(queue, handle, nullptr) == 0 && sceVideoOutAddVblankEvent(queue, handle, &settings) == 0, "vblank subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->vblankEvents.size() == 1, "duplicate vblank subscription");
    }
    check(sceVideoOutAddVrrActiveStatusEvent(queue, handle, &settings) == 0, "VRR status subscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->vrrStatusEvents.size() == 1, "VRR status subscription was not recorded");
    }
    check(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    check(owner->GetTriggeredEvents(&event, 1) == 1 && event.udata == &settings && sceVideoOutGetEventId(&event) == VIDEO_OUT_EVENT_VBLANK, "vblank event or updated user data missing");
    check(sceVideoOutAddPreVblankStartEvent(queue, handle, &mode) == 0, "pre-vblank subscription failed");
    check(sceVideoOutWaitVblank(handle) == 0, "vblank wait failed");
    std::array<KernelEvent, 8> events{};
    const int collected = owner->GetTriggeredEvents(events.data(), static_cast<int>(events.size()));
    check(collected >= 2, "pre-vblank or vblank event missing");
    const auto end = events.begin() + collected;
    check(std::any_of(events.begin(), end, [&](KernelEvent& item) { return sceVideoOutGetEventId(&item) == VIDEO_OUT_EVENT_VBLANK && item.udata == &settings; }), "vblank event missing");
    check(std::any_of(events.begin(), end, [&](KernelEvent& item) { return sceVideoOutGetEventId(&item) == VIDEO_OUT_EVENT_PRE_VBLANK_START && item.udata == &mode; }), "pre-vblank event missing");
    check(sceVideoOutDeletePreVblankStartEvent(queue, handle) == 0, "pre-vblank unsubscription failed");
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {allocation.data(), storage.data()}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "buffers with set reserved pointers were rejected");
    check(sceVideoOutUnregisterBuffers(handle, 0) == 0, "buffer unregistration failed");
    check(owner->GetTriggeredEvents(&event, 1) == 0, "VRR status event triggered without a VRR change");
    KernelEvent vrrStatus{};
    vrrStatus.ident = VIDEO_OUT_EVENT_VRR_STATUS;
    vrrStatus.filter = EVFILT_VIDEO_OUT;
    check(sceVideoOutGetEventId(&vrrStatus) == VIDEO_OUT_EVENT_VRR_STATUS, "VRR status event id rejected");
    check(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0, "VRR peg on an open handle failed");
    check(sceVideoOutVrrUnpegFromFixedRate(handle) == 0, "VRR unpeg on an open handle failed");
    check(sceVideoOutDeleteVrrActiveStatusEvent(queue, handle) == 0, "VRR status unsubscription failed");
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->vrrStatusEvents.empty(), "VRR status unsubscription did not clear registration");
    }
    check(sceVideoOutDeleteVrrActiveStatusEvent(queue, handle) == 0, "duplicate VRR status unsubscription failed");
    expectFailure([&] { sceVideoOutDeleteVrrActiveStatusEvent(0, handle); });
    expectFailure([&] { sceVideoOutDeleteVrrActiveStatusEvent(queue, -1); });
    expectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 1, 0); });
    expectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 1); });
    expectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0x100000000ull, 0); });
    expectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0x100000000ull); });
    sceVideoOutClose(handle);
    expectFailure([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 0); });
    expectFailure([&] { sceVideoOutVrrUnpegFromFixedRate(handle); });
    check(owner->GetTriggeredEvents(&event, 1) == 0, "closed port retained pending events");
    check(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    LibcRunShutdown_nid_postfix();
}

void testPresentation() {
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

void testOverlay() {
    const int mainHandle = sceVideoOutOpen(255, 0, 0, nullptr);
    const int overlayHandle = sceVideoOutOpen(255, 1, 0, nullptr);
    check(mainHandle != overlayHandle, "the overlay bus shares the main port");
    auto mainCfg = VideoOutDriver::Get().GetConfig(mainHandle);
    auto overlayCfg = VideoOutDriver::Get().GetConfig(overlayHandle);
    std::vector<std::byte> mainAllocation(6 * 65536 + 65535);
    std::vector<std::byte> overlayAllocation(6 * 65536 + 65535);
    const auto mainStorage = alignedBuffer(mainAllocation);
    const auto overlayStorage = alignedBuffer(overlayAllocation);
    fillBuffer(mainStorage, 259, 137);
    fillBuffer(overlayStorage, 259, 137);
    VideoOutBuffers mainBuffer{mainStorage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBuffers overlayBuffer{overlayStorage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(mainHandle, 0, 0, &mainBuffer, 1, &attribute, 0, nullptr) == 0, "main buffer registration failed");
    check(sceVideoOutRegisterBuffers2(overlayHandle, 0, 0, &overlayBuffer, 1, &attribute, 0, nullptr) == 0, "overlay buffer registration failed");
    const auto flip = [](int handle, const std::shared_ptr<VideoOutConfig>& cfg, std::int64_t argument) {
        std::uint64_t target;
        {
            std::lock_guard lock(cfg->mutex);
            target = cfg->flipStatus.count + 1;
        }
        check(sceVideoOutSubmitFlip(handle, 0, 1, argument) == 0, "the flip was not accepted");
        std::unique_lock lock(cfg->mutex);
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == target; }), "the flip did not complete");
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(cfg->flipStatus.flipArg == argument && cfg->flipStatus.currentBuffer == 0 && cfg->flipStatus.flipPendingNum == 0, "flip status is wrong");
    };
    const auto presentsAfter = [](std::uint64_t previous) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (AgcDriver::VulkanDevice::PresentCounts().presents == previous && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return AgcDriver::VulkanDevice::PresentCounts().presents;
    };
    const auto start = AgcDriver::VulkanDevice::PresentCounts().presents;
    flip(mainHandle, mainCfg, 1);
    const auto afterMain = presentsAfter(start);
    check(afterMain > start, "a main-bus flip was not presented");
    flip(overlayHandle, overlayCfg, 2);
    flip(overlayHandle, overlayCfg, 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    check(AgcDriver::VulkanDevice::PresentCounts().presents == afterMain, "an overlay-bus flip replaced the main output");
    flip(mainHandle, mainCfg, 4);
    check(presentsAfter(afterMain) > afterMain, "a main-bus flip after overlay flips was not presented");
    sceVideoOutUnregisterBuffers(overlayHandle, 0);
    sceVideoOutUnregisterBuffers(mainHandle, 0);
    sceVideoOutClose(overlayHandle);
    sceVideoOutClose(mainHandle);
    LibcRunShutdown_nid_postfix();
}

void testUnavailable() {
    const auto message = expectFailure([] { sceVideoOutOpen(255, 0, 0, nullptr); });
    check(expectFailure([] { sceVideoOutOpen(255, 0, 0, nullptr); }) == message, "a second open lost the presentation failure");
    std::printf("presentation unavailable: %s\n", message.c_str());
    LibcRunShutdown_nid_postfix();
}

void testCompressedPresentation() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
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
        check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == target; }), "DCC presentation did not complete");
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(cfg->flipStatus.flipArg == argument && cfg->flipStatus.flipPendingNum == 0, "DCC flip status is wrong");
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

std::uint32_t busyLoopResult(std::uint32_t iterations) {
    std::uint32_t value = 1;
    for (std::uint32_t i = 0; i < iterations; ++i) value = value * value + 1u;
    return value;
}

struct BusyLoop {
    volatile std::uint32_t* result;
    volatile std::uint32_t* label;
    std::vector<std::uint32_t> commands;
};

constexpr std::uint32_t busyLoopTotalIterations = 100000000u;
constexpr std::uint32_t busyLoopDispatchCount = 100u;
constexpr std::uint32_t busyLoopIterationsPerDispatch = busyLoopTotalIterations / busyLoopDispatchCount;
static_assert(busyLoopTotalIterations % busyLoopDispatchCount == 0);

BusyLoop prepareBusyLoop(std::uint32_t iterations, std::uint32_t dispatchCount = 1u) {
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
    check(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, 65536, 65536, 0, &physical) == 0, "direct memory allocation failed");
    check(sceKernelMapDirectMemory(&mapped, 65536, 0x33, 0, physical, 65536) == 0, "direct memory mapping failed");
    auto* result = &static_cast<volatile std::uint32_t*>(mapped)[0];
    auto* label = &static_cast<volatile std::uint32_t*>(mapped)[64];
    *result = 0;
    *label = 0;
    const auto program = reinterpret_cast<std::uintptr_t>(code.data());
    const auto output = reinterpret_cast<std::uintptr_t>(result);
    const auto target = reinterpret_cast<std::uintptr_t>(label);
    std::vector<std::uint32_t> commands{
        0xc0027600, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600, 0x213, 5u << 1u,
        0xc0037600, 0x207, 32, 1, 1,
        0xc0057600, 0x240, static_cast<std::uint32_t>(output), static_cast<std::uint32_t>(output >> 32u) & 0xffffu, 4, 0x31016fac, iterations,
    };
    for (std::uint32_t dispatch = 0; dispatch < dispatchCount; ++dispatch) {
        commands.insert(commands.end(), {0xc0031500, 1, 1, 1, 0x8041});
    }
    commands.insert(commands.end(), {0xc0064900, 0x514, 1u << 29u, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(target >> 32u), 1, 0, 0});
    return {result, label, std::move(commands)};
}

void testReleaseVblank() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    check(sceVideoOutSetFlipRate(handle, 2) == 0, "flip rate was not applied");
    auto busy = prepareBusyLoop(busyLoopIterationsPerDispatch, busyLoopDispatchCount);
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    sceVideoOutSubmitFlip(handle, 0, 1, 1);
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the flip did not complete");
        check(cfg->lastFlipVblank >= 3, "the first flip was released before its configured interval");
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void testFlipAfterGpuWork(bool eop = false) {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    constexpr std::uint32_t iterations = busyLoopIterationsPerDispatch;
    auto busy = prepareBusyLoop(iterations, busyLoopDispatchCount);
    auto& result = *busy.result;
    auto& label = *busy.label;
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    check(sceVideoOutSubmitFlip(handle, 0, 1, 4) == 0, "the first flip was not accepted");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the first flip did not complete");
    }
    if (eop) {
        std::uint32_t token = 0;
        check(sceVideoOutSubmitEopFlip(handle, 0, 1, 5, &token) == 0, "GPU-work EOP reservation failed");
        const auto target = reinterpret_cast<std::uintptr_t>(busy.label);
        const std::array<std::uint32_t, 8> release{0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(target >> 32u), 1u, 0u, token};
        busy.commands.insert(busy.commands.end(), release.begin(), release.end());
        packet.addr = busy.commands.data();
        packet.dw_num = static_cast<std::uint32_t>(busy.commands.size());
    }
    std::atomic<std::int64_t> landed{0};
    const auto start = std::chrono::steady_clock::now();
    const auto since = [&] { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count(); };
    std::thread poller([&] {
        while (label != 1 && since() < 30000000) std::this_thread::yield();
        if (label == 1) landed.store(since());
    });
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    if (!eop) check(sceVideoOutSubmitFlip(handle, 0, 1, 5) == 0, "the flip was not accepted");
    std::int64_t flipped = 0;
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 2; });
        flipped = since();
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the flip did not complete");
    }
    poller.join();
    AgcDriverWaitIdle_nid_postfix();
    std::printf("GPU work landed at %.1f ms, flip completed at %.1f ms\n", landed.load() / 1000.0, flipped / 1000.0);
    check(landed.load() >= 100000, "the GPU work did not land or was too short to order against the flip");
    check(result == busyLoopResult(iterations), "the GPU work stored the wrong result");
    check(flipped + 20000 >= landed.load(), "the flip completed before the GPU work submitted ahead of it");
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void testLabels(bool overlay) {
    std::uintptr_t address = 0x1234;
    for (const int invalid : {-1, 0, VIDEO_OUT_NUM_MAX, std::numeric_limits<int>::max()}) {
        check(sceVideoOutGetBufferLabelAddress(invalid, &address) == VIDEO_OUT_ERROR_INVALID_HANDLE && address == 0x1234, "invalid label query changed its output");
    }
    check(sceVideoOutGetBufferLabelAddress(1, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS, "null label output was accepted");
    check(sceVideoOutGetBufferLabelAddress(2, &address) == VIDEO_OUT_ERROR_INVALID_HANDLE && address == 0x1234, "unopened label query changed its output");
    const int bus = overlay ? VIDEO_OUT_BUS_TYPE_OVERLAY : VIDEO_OUT_BUS_TYPE_MAIN;
    const int handle = sceVideoOutOpen(255, bus, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    check(sceVideoOutGetBufferLabelAddress(handle, &address) == VIDEO_OUT_BUFFER_NUM_MAX, "label query returned the wrong count");
    check(address != 0 && address % std::atomic_ref<std::uint64_t>::required_alignment == 0, "label array is absent or unaligned");
    auto* labels = reinterpret_cast<std::uint64_t*>(address);
    for (int i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) check(std::atomic_ref(labels[i]).load() == 0, "new port has a nonzero label");
    alignas(std::uintptr_t) std::array<std::byte, sizeof(address) + 2> unaligned;
    unaligned.fill(std::byte{0x5a});
    check(sceVideoOutGetBufferLabelAddress(handle, reinterpret_cast<std::uintptr_t*>(unaligned.data() + 1)) == VIDEO_OUT_BUFFER_NUM_MAX, "unaligned label-address output failed");
    std::uintptr_t repeated = 0;
    std::memcpy(&repeated, unaligned.data() + 1, sizeof(repeated));
    check(repeated == address && unaligned.front() == std::byte{0x5a} && unaligned.back() == std::byte{0x5a}, "label query changed its address or output canaries");
    for (int i : {0, 1, 15}) std::atomic_ref(labels[i]).store(55);
    std::vector<std::byte> allocation(2 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    std::array<VideoOutBuffers, 2> buffers{};
    buffers[0].data = storage.data();
    buffers[1].data = storage.data() + 65536;
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, buffers.data(), 2, &attribute, 0, nullptr) == 0, "label-test buffers failed to register");
    check(std::atomic_ref(labels[0]).load() == 0 && std::atomic_ref(labels[1]).load() == 0 && std::atomic_ref(labels[15]).load() == 55, "registration reset the wrong labels");
    std::atomic_ref(labels[0]).store(1);
    std::atomic_ref(labels[1]).store(1);
    const auto flip = [&](int index, std::uint64_t count) {
        check(sceVideoOutSubmitFlip(handle, index, VIDEO_OUT_FLIP_MODE_VSYNC, index) == 0, "label-test flip failed to submit");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        VideoOutFlipStatus status{};
        do {
            check(sceVideoOutGetFlipStatus(handle, &status) == 0, "label-test flip status failed");
            if (status.count >= count) return;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("label-test flip timed out");
    };
    flip(0, 1);
    check(std::atomic_ref(labels[0]).load() == 1 && std::atomic_ref(labels[1]).load() == 1, "first flip released a label before replacement");
    flip(1, 2);
    check(std::atomic_ref(labels[0]).load() == 0 && std::atomic_ref(labels[1]).load() == 1, "replacement flip released the wrong label");
    flip(VIDEO_OUT_BUFFER_INDEX_BLACK, 3);
    check(std::atomic_ref(labels[1]).load() == 0 && std::atomic_ref(labels[15]).load() == 55, "black flip did not release only the previous buffer");
    flip(VIDEO_OUT_BUFFER_INDEX_BLANK, 4);
    check(std::atomic_ref(labels[15]).load() == 55, "blank flip accessed a negative label index");
    const int otherHandle = sceVideoOutOpen(255, overlay ? VIDEO_OUT_BUS_TYPE_MAIN : VIDEO_OUT_BUS_TYPE_OVERLAY, 0, nullptr);
    std::uintptr_t other = 0;
    check(sceVideoOutGetBufferLabelAddress(otherHandle, &other) == VIDEO_OUT_BUFFER_NUM_MAX && other != address, "ports share their labels");
    sceVideoOutClose(otherHandle);
    sceVideoOutClose(handle);
    repeated = 0x1234;
    check(sceVideoOutGetBufferLabelAddress(handle, &repeated) == VIDEO_OUT_ERROR_INVALID_HANDLE && repeated == 0x1234, "closed label query changed its output");
    const int reopened = sceVideoOutOpen(255, bus, 0, nullptr);
    check(sceVideoOutGetBufferLabelAddress(reopened, &repeated) == VIDEO_OUT_BUFFER_NUM_MAX && repeated != address, "reopened port shares live old-generation labels");
    auto* fresh = reinterpret_cast<std::uint64_t*>(repeated);
    for (int i = 0; i < VIDEO_OUT_BUFFER_NUM_MAX; ++i) check(std::atomic_ref(fresh[i]).load() == 0, "reopened port retained an old label");
    check(std::atomic_ref(labels[15]).load() == 55, "reopening changed old-generation labels");
    sceVideoOutClose(reopened);
    LibcRunShutdown_nid_postfix();
}

void testUnopenedEventRegistration() {
    KernelEqueue eq = 0;
    check(sceKernelCreateEqueue(&eq, "unopened") == 0, "the event queue was not created");
    check(sceVideoOutAddFlipEvent(eq, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE, "flip event on an unopened port was accepted");
    check(sceVideoOutAddVblankEvent(eq, 1, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE, "vblank event on an unopened port was accepted");
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    check(sceVideoOutAddFlipEvent(eq, handle, nullptr) == 0, "flip event on an open port was refused");
    sceVideoOutClose(handle);
    check(sceVideoOutAddFlipEvent(eq, handle, nullptr) == VIDEO_OUT_ERROR_INVALID_HANDLE, "flip event on a closed port was accepted");
    check(sceKernelDeleteEqueue(eq) == 0, "the event queue was not deleted");
    LibcRunShutdown_nid_postfix();
}

void testEop(bool overlay) {
    std::uint32_t token = 0x5a5a5a5a;
    check(sceVideoOutSubmitEopFlip(0, 0, 1, 0, &token) == VIDEO_OUT_ERROR_INVALID_HANDLE && token == 0x5a5a5a5a, "invalid EOP handle changed the token");
    check(sceVideoOutSubmitEopFlip(1, 0, 1, 0, nullptr) == VIDEO_OUT_ERROR_INVALID_ADDRESS, "null EOP token output was accepted");
    const int bus = overlay ? VIDEO_OUT_BUS_TYPE_OVERLAY : VIDEO_OUT_BUS_TYPE_MAIN;
    const int handle = sceVideoOutOpen(255, bus, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    for (const auto [index, mode, error] : {std::array<int, 3>{16, 1, VIDEO_OUT_ERROR_INVALID_INDEX}, {0, 1, VIDEO_OUT_ERROR_INVALID_INDEX}, {-2, 7, VIDEO_OUT_ERROR_INVALID_VALUE}}) {
        check(sceVideoOutSubmitEopFlip(handle, index, mode, 0, &token) == error && token == 0x5a5a5a5a, "invalid EOP parameters changed the token");
    }
    std::vector<std::byte> allocation(65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{};
    buffer.data = storage.data();
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 64;
    attribute.height = 64;
    attribute.pixel_format = 0x8000000000000000ull;
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "EOP-test buffer failed to register");
    alignas(std::uint32_t) std::array<std::byte, 10> output;
    output.fill(std::byte{0x5a});
    constexpr std::int64_t argument = -0x1234567890abcdefLL;
    check(sceVideoOutSubmitEopFlip(handle, 0, 1, argument, reinterpret_cast<std::uint32_t*>(output.data() + 1)) == 0, "EOP flip reservation failed");
    std::memcpy(&token, output.data() + 1, sizeof(token));
    check(token != 0 && token <= 0x0fffffff && output.front() == std::byte{0x5a} && std::all_of(output.begin() + 5, output.end(), [](auto byte) { return byte == std::byte{0x5a}; }), "EOP token overwrote its DWORD output");
    VideoOutFlipStatus status{};
    check(sceVideoOutGetFlipStatus(handle, &status) == 0 && status.flipPendingNum == 1 && status.count == 0, "EOP reservation presented before its completion packet");
    alignas(8) std::uint64_t label = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&label);
    std::array<std::uint32_t, 8> release{0xc0064900u, 0x06200504u, 0x42010000u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 1u, 0u, token};
    auto rejected = release;
    rejected[0] = 0xc0054900u;
    Packet malformed{rejected.data(), static_cast<std::uint32_t>(rejected.size()), 0, {}};
    expectFailure([&] { sceAgcDriverSubmitDcb(&malformed); });
    check(sceVideoOutGetFlipStatus(handle, &status) == 0 && status.flipPendingNum == 1 && status.count == 0, "rejected packet consumed an EOP reservation");
    const auto submit = [](auto& words) {
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        check(sceAgcDriverSubmitDcb(&packet) == 0, "EOP completion packet was rejected");
    };
    std::vector<std::uint32_t> duplicate(release.begin(), release.end());
    duplicate.insert(duplicate.end(), release.begin(), release.end());
    Packet twice{duplicate.data(), static_cast<std::uint32_t>(duplicate.size()), 0, {}};
    check(expectFailure([&] { sceAgcDriverSubmitDcb(&twice); }).find("token repeated") != std::string::npos, "a submission consumed the same EOP token twice");
    const std::uint64_t bits = static_cast<std::uint64_t>(argument);
    std::vector<std::uint32_t> completion{0xc0027900u, 0x342u, 0xc7010107u, static_cast<std::uint32_t>(bus), 0xc0043700u, 0x06010000u, 0xc343u, 0u, static_cast<std::uint32_t>(bits), static_cast<std::uint32_t>(bits >> 32u)};
    completion.push_back(0x80004900u);
    const std::array<std::uint32_t, 8> nop{0xc0061000u, 0x02000000u, 0u, 0u, 0u, 0u, token, 0u};
    completion.insert(completion.end(), nop.begin(), nop.end());
    completion.insert(completion.end(), release.begin(), release.end());
    submit(completion);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        check(sceVideoOutGetFlipStatus(handle, &status) == 0, "EOP flip status failed");
        if (status.count == 1 && status.flipPendingNum == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    check(status.count == 1 && status.flipPendingNum == 0 && status.currentBuffer == 0 && status.flipArg == argument && std::atomic_ref(label).load() == 1, "EOP packet did not finish its label and reserved flip");
    release[5] = 2;
    submit(release);
    AgcDriverWaitIdle_nid_postfix();
    check(sceVideoOutGetFlipStatus(handle, &status) == 0 && status.count == 1 && status.flipPendingNum == 0 && std::atomic_ref(label).load() == 2, "ordinary context ID replay changed the flip or lost its label");
    std::array<std::uint32_t, 32> pending;
    pending.fill(0x5a5a5a5a);
    std::array<int, 32> outcomes{};
    std::vector<std::thread> callers;
    for (std::size_t i = 0; i < pending.size(); ++i) callers.emplace_back([&, i] { outcomes[i] = sceVideoOutSubmitEopFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLACK, 1, 0, &pending[i]); });
    for (auto& caller : callers) caller.join();
    std::vector<std::uint32_t> abandoned;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (outcomes[i] == 0) abandoned.push_back(pending[i]);
        else check(outcomes[i] == VIDEO_OUT_ERROR_FLIP_QUEUE_FULL && pending[i] == 0x5a5a5a5a, "concurrent EOP overflow changed its token");
    }
    std::sort(abandoned.begin(), abandoned.end());
    check(abandoned.size() == VIDEO_OUT_FLIP_QUEUE_CAPACITY && abandoned.front() > token && std::adjacent_find(abandoned.begin(), abandoned.end()) == abandoned.end(), "concurrent EOP reservations exceeded capacity or reused tokens");
    std::uint32_t unchanged = 0x5a5a5a5a;
    check(sceVideoOutSubmitEopFlip(handle, VIDEO_OUT_BUFFER_INDEX_BLACK, 1, 0, &unchanged) == VIDEO_OUT_ERROR_FLIP_QUEUE_FULL && unchanged == 0x5a5a5a5a, "full EOP queue accepted a reservation or changed its output");
    sceVideoOutClose(handle);
    {
        std::lock_guard lock(cfg->mutex);
        check(cfg->flipStatus.flipPendingNum == 0, "closing kept unsubmitted EOP reservations");
    }
    const int reopened = sceVideoOutOpen(255, bus, 0, nullptr);
    release[7] = abandoned.front();
    release[5] = 3;
    submit(release);
    AgcDriverWaitIdle_nid_postfix();
    check(sceVideoOutGetFlipStatus(reopened, &status) == 0 && status.count == 0 && status.flipPendingNum == 0 && std::atomic_ref(label).load() == 3, "cancelled context ID changed the reopened port or lost its label");
    std::uint32_t fresh = 0;
    check(sceVideoOutSubmitEopFlip(reopened, VIDEO_OUT_BUFFER_INDEX_BLACK, 1, 7, &fresh) == 0 && fresh > abandoned.back(), "reopening reused an EOP token");
    sceVideoOutClose(reopened);
    LibcRunShutdown_nid_postfix();
}

void testOneDevice() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::vector<std::byte> allocation(6 * 65536 + 65535);
    const auto storage = alignedBuffer(allocation);
    fillBuffer(storage, 259, 137);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    sceVideoOutSetBufferAttribute2(&attribute, 0x8000000000000000ull, 0, 259, 137, 0, 0, 0);
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    constexpr std::uint32_t iterations = 1000u;
    auto busy = prepareBusyLoop(iterations);
    Packet packet{busy.commands.data(), static_cast<std::uint32_t>(busy.commands.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "the GPU work was not accepted");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (*busy.label != 1 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(*busy.label == 1 && *busy.result == busyLoopResult(iterations), "the GPU work did not run");
    check(sceVideoOutSubmitFlip(handle, 0, 1, 1) == 0, "the flip was not accepted");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(30), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the flip did not complete");
    }
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
    const AgcDriver::VulkanDevice next;
    check(next.Serial() == 2, "the GPU work and the flip used more than one device");
}

void testOutputResolution() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    SDL_DisplayMode mode{};
    check(SDL_GetCurrentDisplayMode(0, &mode) == 0, "the display mode could not be read");
    const std::uint32_t expected = (mode.w >= 3840 || mode.h >= 2160) ? 2u : 1u;
    VideoOutOutputStatus status{};
    check(sceVideoOutGetOutputStatus(handle, &status) == 0 && status.resolution == expected, "a port without buffers did not report the display's resolution");
    const std::uint32_t width = expected == 2u ? 1920u : 3840u;
    const std::uint32_t height = expected == 2u ? 1080u : 2160u;
    std::vector<std::byte> allocation(static_cast<std::size_t>(width) * height * 4 + 65535);
    const auto storage = alignedBuffer(allocation);
    VideoOutBuffers buffer{storage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = width;
    attribute.height = height;
    attribute.tiling_mode = 1;
    attribute.pitch_in_pixel = width;
    attribute.pixel_format = 0x8000000000000000ull;
    sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr);
    check(sceVideoOutSubmitFlip(handle, 0, 1, 1) == 0, "the flip was not accepted");
    {
        std::unique_lock lock(cfg->mutex);
        const bool done = cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure || cfg->flipStatus.count == 1; });
        if (cfg->failure) std::rethrow_exception(cfg->failure);
        check(done, "the flip did not complete");
        check(cfg->width == width && cfg->height == height, "the port did not record the buffer size");
    }
    check(sceVideoOutGetOutputStatus(handle, &status) == 0 && status.resolution == expected, "the flipped buffer's size changed the reported resolution");
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

void setOutputResolution(const char* value) {
#ifdef _WIN32
    check(_putenv_s("APS5_OUTPUT_RESOLUTION", value) == 0, "APS5_OUTPUT_RESOLUTION was not set");
#else
    check((*value != '\0' ? setenv("APS5_OUTPUT_RESOLUTION", value, 1) : unsetenv("APS5_OUTPUT_RESOLUTION")) == 0, "APS5_OUTPUT_RESOLUTION was not set");
#endif
}

void testOutputResolutionOverride() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    SDL_DisplayMode mode{};
    check(SDL_GetCurrentDisplayMode(0, &mode) == 0, "the display mode could not be read");
    const std::uint32_t following = (mode.w >= 3840 || mode.h >= 2160) ? 2u : 1u;
    const auto reported = [&] {
        VideoOutOutputStatus status{};
        check(sceVideoOutGetOutputStatus(handle, &status) == 0, "the output status was not returned");
        return status.resolution;
    };
    setOutputResolution("");
    check(reported() == following, "an unset override changed the display's resolution");
    const std::pair<const char*, std::uint32_t> overrides[] = {{"1920x1080", 1u}, {"1280x720", 1u}, {"3840x2160", 2u}, {"2560x2160", 2u}, {"3840X1", 2u}};
    for (const auto& [value, expected] : overrides) {
        setOutputResolution(value);
        check(reported() == expected, (std::string("the override ") + value + " did not set the reported resolution").c_str());
        std::array<std::byte, 80> raw{};
        VideoOutOutputStatus withVrr{};
        check(sceVideoOutGetOutputStatusWithVrr(handle, raw.data()) == 0, "the VRR status was not returned");
        std::memcpy(&withVrr, raw.data(), sizeof(withVrr));
        check(withVrr.resolution == expected,(std::string("the override ") + value + " did not reach the VRR status").c_str());
    }
    for (const char* value : {"1920", "1920x", "x1080", "0x1080", "1920x0", "1920x1080x2", " 1920x1080", "1920x1080 ", "-1920x1080", "1920*1080", "4294967296x1080", "high"}) {
        setOutputResolution(value);
        const auto message = expectFailure([&] { reported(); });
        check(message.find("APS5_OUTPUT_RESOLUTION") != std::string::npos && message.find(value) != std::string::npos, (std::string("the malformed value ") + value + " was not rejected by name").c_str());
    }
    setOutputResolution("");
    check(reported() == following, "clearing the override did not restore the display's resolution");
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

}

int main(int argc, char** argv) {
    VideoOutTestEnvironment environment;
    try {
        if (argc == 2 && std::string(argv[1]) == "decode") testDecode();
        else if (argc == 2 && std::string(argv[1]) == "controls") testControls();
        else if (argc == 2 && std::string(argv[1]) == "present") testPresentation();
        else if (argc == 2 && std::string(argv[1]) == "backtoback") testBackToBack();
        else if (argc == 2 && std::string(argv[1]) == "dcc") testCompressedPresentation();
        else if (argc == 2 && std::string(argv[1]) == "pacing") testReleaseVblank();
        else if (argc == 2 && std::string(argv[1]) == "aftergpu") testFlipAfterGpuWork();
        else if (argc == 2 && std::string(argv[1]) == "unavailable") testUnavailable();
        else if (argc == 2 && std::string(argv[1]) == "labels") testLabels(true);
        else if (argc == 2 && std::string(argv[1]) == "labels_main") testLabels(false);
        else if (argc == 2 && std::string(argv[1]) == "onedevice") testOneDevice();
        else if (argc == 2 && std::string(argv[1]) == "unopened") testUnopenedEventRegistration();
        else if (argc == 2 && std::string(argv[1]) == "eop_aftergpu") testFlipAfterGpuWork(true);
        else if (argc == 2 && std::string(argv[1]) == "eop") testEop(true);
        else if (argc == 2 && std::string(argv[1]) == "eop_main") testEop(false);
        else if (argc == 2 && std::string(argv[1]) == "overlay") testOverlay();
        else if (argc == 2 && std::string(argv[1]) == "deferred") testDeferredBuffer();
        else if (argc == 2 && std::string(argv[1]) == "unregistered") testUnregisteredBuffer();
        else if (argc == 2 && std::string(argv[1]) == "ps4") testPs4Buffers();
        else if (argc == 2 && std::string(argv[1]) == "cpuflip") testCpuFlipOrder();
        else if (argc == 2 && std::string(argv[1]) == "resolution") testOutputResolution();
        else if (argc == 2 && std::string(argv[1]) == "resolution_override") testOutputResolutionOverride();
        else testLifetime(argc == 2 && std::string(argv[1]) == "reopen");
        std::puts("VideoOut flip tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        const std::string what = error.what();
        const bool noVulkanWindow = what.find("Vulkan support") != std::string::npos || (what.starts_with("DisplayWindow: ") && what.find("Vulkan") != std::string::npos);
        if (noVulkanWindow && !std::getenv("ANYPS5_REQUIRE_DISPLAY")) {
            std::printf("skipped, no display or Vulkan device: %s\n", error.what());
            return 77;
        }
        return 1;
    }
}
