#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <limits>

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

void* lastFreed = nullptr;
int frees = 0;
bool failAllocation = false;
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;
using DecoderGetResolution = std::int64_t (APS5_VABI*)(void*, float*, float*);
int resolutionCalls = 0;
void* resolutionDecoder = nullptr;
float* resolutionX = nullptr;
float* resolutionY = nullptr;
std::int64_t resolutionResult = 0;
bool throwResolution = false;
std::int64_t APS5_VABI GetResolution(void* decoder, float* x, float* y) {
    ++resolutionCalls;
    resolutionDecoder = decoder;
    resolutionX = x;
    resolutionY = y;
    if (throwResolution) throw std::runtime_error("resolution callback");
    if (x) *x = 17.25f;
    if (y) *y = 91.5f;
    return resolutionResult;
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    lastCount = count;
    lastSize = size;
    lastAllocated = failAllocation ? nullptr : std::calloc(count, size);
    return lastAllocated;
}
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void TestFormatConverterGetResolution() {
    alignas(8) std::array<std::byte, 32> decoder{};
    const DecoderGetResolution callback = &GetResolution;
    std::memcpy(decoder.data() + 24, &callback, sizeof(callback));
    JxrGuestFormatConverter converter{};
    converter.decoder = decoder.data();
    std::array<std::byte, sizeof(converter)> converterBefore{};
    std::memcpy(converterBefore.data(), &converter, sizeof(converter));
    const auto decoderBefore = decoder;
    struct GuardedOutputs {
        std::uint64_t beforeX;
        float x;
        std::uint64_t between;
        float y;
        std::uint64_t afterY;
    } outputs{0x123456789abcdef0, 0, 0x0fedcba987654321, 0, 0x8877665544332211};
    resolutionCalls = 0;
    resolutionResult = INT64_C(0x123456789abcdef);
    Require(PKFormatConverter_GetResolution_nid_postfix(&converter, &outputs.x, &outputs.y) == resolutionResult);
    Require(resolutionCalls == 1 && resolutionDecoder == decoder.data());
    Require(resolutionX == &outputs.x && resolutionY == &outputs.y);
    Require(outputs.x == 17.25f && outputs.y == 91.5f);
    Require(outputs.beforeX == 0x123456789abcdef0 && outputs.between == 0x0fedcba987654321 && outputs.afterY == 0x8877665544332211);
    Require(std::memcmp(&converter, converterBefore.data(), sizeof(converter)) == 0);
    Require(decoder == decoderBefore);

    resolutionResult = -INT64_C(0x123456789abcdef);
    Require(PKFormatConverter_GetResolution_nid_postfix(&converter, nullptr, nullptr) == resolutionResult);
    Require(resolutionCalls == 2 && resolutionDecoder == decoder.data() && resolutionX == nullptr && resolutionY == nullptr);
    Require(std::memcmp(&converter, converterBefore.data(), sizeof(converter)) == 0);
    Require(decoder == decoderBefore);

    throwResolution = true;
    bool callbackThrew = false;
    try { PKFormatConverter_GetResolution_nid_postfix(&converter, nullptr, nullptr); }
    catch (const std::runtime_error&) { callbackThrew = true; }
    throwResolution = false;
    Require(callbackThrew && resolutionCalls == 3);
    Require(std::memcmp(&converter, converterBefore.data(), sizeof(converter)) == 0);
    Require(decoder == decoderBefore);

    const auto requireInvalid = [](JxrGuestFormatConverter* value) {
        bool threw = false;
        try { PKFormatConverter_GetResolution_nid_postfix(value, nullptr, nullptr); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    };
    requireInvalid(nullptr);
    requireInvalid(reinterpret_cast<JxrGuestFormatConverter*>(std::uintptr_t{1}));
    requireInvalid(reinterpret_cast<JxrGuestFormatConverter*>(std::numeric_limits<std::uintptr_t>::max()));
    JxrGuestFormatConverter nullDecoder{};
    requireInvalid(&nullDecoder);
    alignas(8) std::array<std::byte, 32> nullCallbackDecoder{};
    JxrGuestFormatConverter nullCallbackConverter{};
    nullCallbackConverter.decoder = nullCallbackDecoder.data();
    requireInvalid(&nullCallbackConverter);
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
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

}

int main() {
    InitializeHeap();
    TestFormatConverterGetResolution();
    void* memory = malloc_nid_postfix(64);
    Require(memory != nullptr);
    void* const allocated = memory;
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1 && lastFreed == allocated);
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1);
    Require(PKFree_nid_postfix(nullptr) == 0);
    Require(frees == 1);
    for (const auto size : {std::size_t{1}, std::size_t{257}, std::size_t{4097}, std::size_t{0}}) {
        memory = &memory;
        const auto calls = callocCalls;
        const auto result = PKAlloc_nid_postfix(&memory, size);
        Require(callocCalls == calls + 1 && lastCount == 1 && lastSize == size);
        Require(memory == lastAllocated);
        Require(result == (memory ? 0 : -101));
        if (size != 0) Require(memory != nullptr);
        auto* bytes = static_cast<unsigned char*>(memory);
        for (std::size_t i = 0; i < size; ++i) Require(bytes[i] == 0);
        if (size != 0) std::memset(memory, 0xa5, size);
        const auto previousFrees = frees;
        const auto pointer = memory;
        Require(PKFree_nid_postfix(&memory) == 0 && memory == nullptr);
        Require(frees == previousFrees + (pointer ? 1 : 0));
        if (pointer) Require(lastFreed == pointer);
    }
    failAllocation = true;
    memory = &memory;
    Require(PKAlloc_nid_postfix(&memory, 37) == -101);
    Require(memory == nullptr && lastCount == 1 && lastSize == 37);
    failAllocation = false;
    bool threw = false;
    try { PKAlloc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    return 0;
}
