#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr int ErrorInvalidAddress = static_cast<int>(0x80290002u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int ErrorNoBuffersRegistered = static_cast<int>(0x8029001Cu);

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUnopenedHandleBounds() {
    std::array<std::uint8_t, 80> vrrBuffer{};
    vrrBuffer.fill(0xAA);

    VideoOutResolutionStatus resStatus{};
    VideoOutOutputStatus outStatus{};

    for (const int badHandle : {-1000, -100, -1, 0, 1, 2, 8, 100, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutGetOutputStatusWithVrr(badHandle, nullptr) == ErrorInvalidAddress);

        Require(sceVideoOutGetOutputStatusWithVrr(badHandle, vrrBuffer.data()) == ErrorInvalidHandle);
        Require(std::all_of(vrrBuffer.begin(), vrrBuffer.end(), [](std::uint8_t b) { return b == 0xAA; }));

        Require(Throws([badHandle] { return sceVideoOutGetResolutionStatus(badHandle, nullptr); }));
        Require(Throws([&] { return sceVideoOutGetResolutionStatus(badHandle, &resStatus); }));

        Require(Throws([badHandle] { return sceVideoOutGetOutputStatus(badHandle, nullptr); }));
        Require(Throws([&] { return sceVideoOutGetOutputStatus(badHandle, &outStatus); }));
    }
}

void TestOpenHandleBounds(int handle) {
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == ErrorInvalidAddress);
    Require(Throws([handle] { return sceVideoOutGetResolutionStatus(handle, nullptr); }));
    Require(Throws([handle] { return sceVideoOutGetOutputStatus(handle, nullptr); }));

    static_assert(sizeof(VideoOutResolutionStatus) == 48);
    static_assert(sizeof(VideoOutOutputStatus) == 48);

    std::array<std::uint8_t, 80> vrrBuffer{};
    vrrBuffer.fill(0x5A);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == ErrorNoBuffersRegistered);
    Require(std::all_of(vrrBuffer.begin(), vrrBuffer.end(), [](std::uint8_t b) { return b == 0x5A; }));

    VideoOutOutputStatus output{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(output.resolution == 1u || output.resolution == 2u);
    Require(output.dynamicRange == 0u || output.dynamicRange == 1u);
    Require(output.flags == 0);

    VideoOutResolutionStatus res{};
    Require(sceVideoOutGetResolutionStatus(handle, &res) == 0);
    Require(res.fullWidth > 0);
    Require(res.fullHeight > 0);
    Require(res.paneWidth == res.fullWidth);
    Require(res.paneHeight == res.fullHeight);
    Require(res.screenSizeInInch == 0.0f);
    Require(res.flags == 0);
    Require(res.reserved0 == 0);
    for (std::size_t i = 0; i < 3; ++i) {
        Require(res.reserved1[i] == 0);
    }

    std::vector<std::byte> allocation(16u * 1024u * 1024u);
    const auto aligned = (reinterpret_cast<std::uintptr_t>(allocation.data()) + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffers{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 1920;
    attribute.height = 1080;
    attribute.pixel_format = 0x8000000022000000ull;
    attribute.tiling_mode = 0;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffers, 1, &attribute, 0, nullptr) == 0);
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0);

    vrrBuffer.fill(0x5A);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == 0);

    VideoOutOutputStatus vrrOutput{};
    std::memcpy(&vrrOutput, vrrBuffer.data(), sizeof(vrrOutput));
    Require(vrrOutput.resolution == 1u || vrrOutput.resolution == 2u);
    Require(vrrOutput.dynamicRange == 0u || vrrOutput.dynamicRange == 1u);
    Require(vrrOutput.flags == 0);
    for (std::size_t i = 48; i < 80; ++i) {
        Require(vrrBuffer[i] == 0x00);
    }

    Require(sceVideoOutGetOutputStatus(handle, &output) == 0);
    Require(output.resolution == vrrOutput.resolution);
    Require(output.dynamicRange == vrrOutput.dynamicRange);
    Require(output.refreshRate == vrrOutput.refreshRate);
    Require(output.flags == 0);
}

void TestClosedHandleBounds(int handle) {
    std::array<std::uint8_t, 80> vrrBuffer{};
    vrrBuffer.fill(0x33);

    VideoOutResolutionStatus resStatus{};
    VideoOutOutputStatus outStatus{};

    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == ErrorInvalidAddress);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrrBuffer.data()) == ErrorInvalidHandle);
    Require(std::all_of(vrrBuffer.begin(), vrrBuffer.end(), [](std::uint8_t b) { return b == 0x33; }));

    Require(Throws([handle] { return sceVideoOutGetResolutionStatus(handle, nullptr); }));
    Require(Throws([&] { return sceVideoOutGetResolutionStatus(handle, &resStatus); }));

    Require(Throws([handle] { return sceVideoOutGetOutputStatus(handle, nullptr); }));
    Require(Throws([&] { return sceVideoOutGetOutputStatus(handle, &outStatus); }));
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleBounds(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestClosedHandleBounds(handle);

    return 0;
}
