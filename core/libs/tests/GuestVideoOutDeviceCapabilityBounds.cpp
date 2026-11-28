#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr int ErrorInvalidValue = static_cast<int>(0x80290001u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);

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

void TestModeSetAnyBounds() {
    Require(Throws([] { sceVideoOutModeSetAny_(nullptr, 16); }));
    Require(Throws([] { sceVideoOutModeSetAny_(nullptr, 0); }));

    std::array<std::uint8_t, 128> buffer{};

    buffer.fill(0x5A);
    sceVideoOutModeSetAny_(buffer.data(), 0);
    std::uint32_t readSize = 0;
    std::memcpy(&readSize, buffer.data(), sizeof(readSize));
    Require(readSize == 0);
    Require(buffer[4] == 0x5A);

    for (const std::uint32_t size : {4u, 8u, 12u, 16u, 24u, 32u, 48u, 64u, 128u}) {
        buffer.fill(0xAA);
        sceVideoOutModeSetAny_(buffer.data(), size);
        std::memcpy(&readSize, buffer.data(), sizeof(readSize));
        Require(readSize == size);
        for (std::size_t i = 4; i < size; ++i) {
            Require(buffer[i] == 0xFF);
        }
        if (size < buffer.size()) {
            Require(buffer[size] == 0xAA);
        }
    }
}

void TestUnopenedHandlePrecedence() {
    std::array<std::uint8_t, 32> infoBuffer{};
    infoBuffer.fill(0xEE);

    for (const int badHandle : {-100, -1, 0, 1, 2, 8, 100, 9999, 0x7FFFFFFF}) {
        Require(sceVideoOutGetDeviceCapabilityInfo_(badHandle, nullptr, 0) == ErrorInvalidHandle);
        Require(sceVideoOutGetDeviceCapabilityInfo_(badHandle, nullptr, 8) == ErrorInvalidHandle);
        Require(sceVideoOutGetDeviceCapabilityInfo_(badHandle, infoBuffer.data(), 0) == ErrorInvalidHandle);
        Require(sceVideoOutGetDeviceCapabilityInfo_(badHandle, infoBuffer.data(), 8) == ErrorInvalidHandle);
        Require(sceVideoOutGetDeviceCapabilityInfo_(badHandle, infoBuffer.data(), 16) == ErrorInvalidHandle);
        Require(infoBuffer[0] == 0xEE);
    }
}

void TestOpenHandleOperations(int handle) {
    std::array<std::uint8_t, 32> infoBuffer{};
    infoBuffer.fill(0x77);

    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 8) == ErrorInvalidValue);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 0) == ErrorInvalidValue);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 16) == ErrorInvalidValue);

    for (const std::uint64_t invalidSize : {0ull, 1ull, 2ull, 3ull, 4ull, 7ull, 9ull, 10ull, 16ull, 32ull, 64ull, 0xFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull}) {
        infoBuffer.fill(0x77);
        Require(sceVideoOutGetDeviceCapabilityInfo_(handle, infoBuffer.data(), invalidSize) == ErrorInvalidValue);
        Require(infoBuffer[0] == 0x77);
    }

    infoBuffer.fill(0x77);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, infoBuffer.data(), 8) == 0);
    std::uint64_t capability = 0;
    std::memcpy(&capability, infoBuffer.data(), sizeof(capability));
    Require(capability == 1ull);
    Require(infoBuffer[8] == 0x77);
}

void TestPostCloseGates(int handle) {
    std::array<std::uint8_t, 32> infoBuffer{};
    infoBuffer.fill(0x33);

    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, infoBuffer.data(), 8) == ErrorInvalidHandle);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, 8) == ErrorInvalidHandle);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, infoBuffer.data(), 0) == ErrorInvalidHandle);
    Require(infoBuffer[0] == 0x33);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestModeSetAnyBounds();
    TestUnopenedHandlePrecedence();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestOpenHandleOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestPostCloseGates(handle);

    return 0;
}
