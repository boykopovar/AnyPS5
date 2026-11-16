#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
std::size_t JxrDecodeFixtureSize();
std::size_t JxrDecodeFixtureInitializeOffset();
std::size_t JxrDecodeFixtureWmpOffset();
std::int32_t JxrDecodeFixtureCallbacksValid(void* value);
std::int32_t JxrDecodeFixtureZeroState(void* value);
std::int32_t JxrDecodeFixtureFormatMatches(const void* value);
std::int64_t JxrDecodeFixtureCallGetPixelFormat(void* value, void* format);
std::int64_t JxrDecodeFixtureCallGetSize(void* value, std::int32_t* width, std::int32_t* height);
std::int64_t JxrDecodeFixtureCallGetResolution(void* value, float* x, float* y);
std::int64_t JxrDecodeFixtureCallGetColorContext(void* value, std::uint8_t* buffer, std::uint32_t* size);
std::int64_t JxrDecodeFixtureCallGetMetadata(void* value, void* metadata);
std::int64_t JxrDecodeFixtureCallCopy(void* value, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride);
std::int64_t JxrDecodeFixtureCallGetFrameCount(void* value, std::uint32_t* count);
std::int64_t JxrDecodeFixtureCallSelectFrame(void* value, std::uint32_t frame);
std::int64_t JxrDecodeFixtureCallInitialize(void* value, void* stream);
std::int64_t JxrDecodeFixtureCallRelease(void** value);
void* JxrDecodeFixtureCreateStream(std::size_t position, std::int64_t getPosResult, std::int64_t closeResult);
void JxrDecodeFixtureDestroyStream(void* stream);
std::size_t JxrDecodeFixtureGetPosCalls();
std::size_t JxrDecodeFixtureCloseCalls();
std::int32_t JxrDecodeFixtureCloseSlotMatches();
void* JxrDecodeFixtureStream(void* decoder);
std::size_t JxrDecodeFixtureOffset(void* decoder);
std::int32_t JxrDecodeFixtureInitializedFields(void* value);
void JxrDecodeFixtureFillMisc(void* value, int byte);
std::int32_t JxrDecodeFixtureMiscIs(void* value, int byte);
void JxrDecodeFixtureSetStreamOwner(void* value, int owner);
}

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxrDecodeCreate.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

bool failCalloc = false;
std::size_t callocCalls = 0;
std::size_t lastCallocCount = 0;
std::size_t lastCallocSize = 0;
void* lastCalloc = nullptr;
std::size_t freeCalls = 0;

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    if (pointer) ++freeCalls;
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    lastCallocCount = count;
    lastCallocSize = size;
    lastCalloc = failCalloc ? nullptr : std::calloc(count, size);
    return lastCalloc;
}
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

void TestCreationAndCallbacks() {
    void* decoder = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    const auto callsBefore = callocCalls;
    Require(PKImageDecode_Create_nid_postfix(&decoder) == 0);
    Require(callocCalls == callsBefore + 1 && lastCallocCount == 1 && lastCallocSize == JxrDecodeFixtureSize());
    Require(decoder == lastCalloc && decoder != nullptr);
    Require(JxrDecodeFixtureInitializeOffset() == 0);
    Require(JxrDecodeFixtureWmpOffset() > JxrDecodeFixtureInitializeOffset());
    Require(JxrDecodeFixtureCallbacksValid(decoder) && JxrDecodeFixtureZeroState(decoder));

    struct GuardedI32 { std::uint32_t before; std::int32_t value; std::uint32_t after; } width{0x12345678, -1, 0x87654321}, height{0x11223344, -1, 0x44332211};
    Require(JxrDecodeFixtureCallGetSize(decoder, &width.value, &height.value) == 0);
    Require(width.value == 0 && height.value == 0 && width.before == 0x12345678 && width.after == 0x87654321);
    Require(height.before == 0x11223344 && height.after == 0x44332211);
    struct GuardedFloat { std::uint32_t before; float value; std::uint32_t after; } x{0xa1a2a3a4, -1, 0xa5a6a7a8}, y{0xb1b2b3b4, -1, 0xb5b6b7b8};
    Require(JxrDecodeFixtureCallGetResolution(decoder, &x.value, &y.value) == 0);
    Require(x.value == 0 && y.value == 0 && x.before == 0xa1a2a3a4 && x.after == 0xa5a6a7a8);
    Require(y.before == 0xb1b2b3b4 && y.after == 0xb5b6b7b8);
    struct GuardedFormat { std::uint32_t before; std::array<std::uint8_t, 16> value; std::uint32_t after; } format{0x1234abcd, {}, 0xdcba4321};
    format.value.fill(0xa5);
    Require(JxrDecodeFixtureCallGetPixelFormat(decoder, format.value.data()) == 0);
    for (const auto byte : format.value) Require(byte == 0);
    Require(format.before == 0x1234abcd && format.after == 0xdcba4321);
    std::uint32_t frames = 0xffffffff;
    Require(JxrDecodeFixtureCallGetFrameCount(decoder, &frames) == 0 && frames == 0);
    Require(JxrDecodeFixtureCallGetColorContext(decoder, nullptr, nullptr) == -2);
    Require(JxrDecodeFixtureCallGetMetadata(decoder, nullptr) == -2);
    Require(JxrDecodeFixtureCallCopy(decoder, nullptr, nullptr, 0) == -3);
    Require(JxrDecodeFixtureCallSelectFrame(decoder, 7) == 0);

    void* stream = JxrDecodeFixtureCreateStream(0x123456789abcdef0ULL, 0, -88);
    Require(stream != nullptr);
    JxrDecodeFixtureFillMisc(decoder, 0xa5);
    Require(JxrDecodeFixtureCallInitialize(decoder, stream) == 0);
    Require(JxrDecodeFixtureGetPosCalls() == 1 && JxrDecodeFixtureStream(decoder) == stream);
    Require(JxrDecodeFixtureOffset(decoder) == 0x123456789abcdef0ULL);
    Require(JxrDecodeFixtureInitializedFields(decoder) && JxrDecodeFixtureMiscIs(decoder, 0));
    Require(JxrDecodeFixtureCallGetPixelFormat(decoder, format.value.data()) == 0);
    Require(JxrDecodeFixtureFormatMatches(format.value.data()));
    JxrDecodeFixtureSetStreamOwner(decoder, 1);
    const auto freesBefore = freeCalls;
    Require(JxrDecodeFixtureCallRelease(&decoder) == 0 && decoder == nullptr);
    Require(JxrDecodeFixtureCloseCalls() == 1 && freeCalls == freesBefore + 1);
    Require(JxrDecodeFixtureCloseSlotMatches());
    JxrDecodeFixtureDestroyStream(stream);
}

void TestErrorsAndPartialInitialization() {
    failCalloc = true;
    void* decoder = reinterpret_cast<void*>(std::uintptr_t{0xdead});
    const auto callsBefore = callocCalls;
    Require(PKImageDecode_Create_nid_postfix(&decoder) == -101);
    Require(callocCalls == callsBefore + 1 && lastCallocCount == 1 && lastCallocSize == JxrDecodeFixtureSize());
    Require(decoder == nullptr);
    failCalloc = false;

    bool threw = false;
    try { PKImageDecode_Create_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    Require(PKImageDecode_Create_nid_postfix(&decoder) == 0 && decoder != nullptr);
    auto* stream = JxrDecodeFixtureCreateStream(0x8877665544332211ULL, -(std::int64_t{1} << 40), 0);
    Require(stream != nullptr);
    JxrDecodeFixtureFillMisc(decoder, 0x5a);
    Require(JxrDecodeFixtureCallInitialize(decoder, stream) == 0);
    Require(JxrDecodeFixtureGetPosCalls() == 1 && JxrDecodeFixtureStream(decoder) == stream);
    Require(JxrDecodeFixtureOffset(decoder) == 0 && JxrDecodeFixtureInitializedFields(decoder));
    Require(JxrDecodeFixtureMiscIs(decoder, 0x5a));
    Require(JxrDecodeFixtureCallRelease(&decoder) == 0 && decoder == nullptr);
    Require(JxrDecodeFixtureCloseCalls() == 0);
    JxrDecodeFixtureDestroyStream(stream);
}

}

int main() {
    InitializeHeap();
    TestCreationAndCallbacks();
    TestErrorsAndPartialInitialization();
    return 0;
}
