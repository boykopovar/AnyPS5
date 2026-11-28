#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "application heap lifecycle: %s\n", message);
        std::abort();
    }
}

alignas(64) std::array<std::byte, 64> heapStorage{};
unsigned initializeCalls = 0;
unsigned finalizeCalls = 0;
unsigned freeCalls = 0;
unsigned realignCalls = 0;
const void* lastFreedPointer = nullptr;
const void* realignLastPointer = &heapStorage;
std::size_t realignLastBytes = 123;
std::size_t realignLastAlignment = 0;
bool initializeShouldThrow = false;
bool realignReturnNull = false;

void APS5_VABI HeapInitialize() {
    ++initializeCalls;
    if (initializeShouldThrow) throw std::runtime_error("custom heap init failure");
}

void APS5_VABI HeapFinalize() {
    ++finalizeCalls;
    ApplicationHeapFinalize_nid_no_patch();
}

void* APS5_VABI HeapAllocate(std::size_t) { return heapStorage.data(); }

void APS5_VABI HeapFree(void* pointer) {
    ++freeCalls;
    lastFreedPointer = pointer;
}

void* APS5_VABI HeapCalloc(std::size_t, std::size_t) { return heapStorage.data(); }
void* APS5_VABI HeapReallocate(void*, std::size_t) { return heapStorage.data(); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t) { return heapStorage.data(); }

void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t alignment) {
    ++realignCalls;
    realignLastPointer = pointer;
    realignLastBytes = bytes;
    realignLastAlignment = alignment;
    return realignReturnNull ? nullptr : heapStorage.data();
}

int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t) {
    *pointer = heapStorage.data();
    return 0;
}

template<typename TValue, std::size_t TSize>
void WriteField(std::array<std::byte, TSize>& buffer, std::size_t offset, TValue value) {
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}

const void* BuildProcessParameters() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    WriteField(process, 0, std::uint64_t{0x40});
    WriteField(process, 8, std::uint32_t{0x4942524f});
    WriteField(process, 0x38, libc.data());
    WriteField(libc, 0, std::uint64_t{0x38});
    WriteField(libc, 0x30, replacement.data());
    WriteField(replacement, 0, std::uint64_t{0x78});
    WriteField(replacement, 8, std::uint64_t{2});
    WriteField(replacement, 0x10, &HeapInitialize);
    WriteField(replacement, 0x18, &HeapFinalize);
    WriteField(replacement, 0x20, &HeapAllocate);
    WriteField(replacement, 0x28, &HeapFree);
    WriteField(replacement, 0x30, &HeapCalloc);
    WriteField(replacement, 0x38, &HeapReallocate);
    WriteField(replacement, 0x40, &HeapAlign);
    WriteField(replacement, 0x48, &HeapRealign);
    WriteField(replacement, 0x50, &HeapPosixAlign);
    return process.data();
}

void CheckInitFailure() {
    initializeShouldThrow = true;
    const void* process = BuildProcessParameters();
    bool initThrew = false;
    try {
        ApplicationHeapInitialize_nid_no_patch(process);
    } catch (const std::runtime_error&) {
        initThrew = true;
    }
    Require(initThrew && initializeCalls == 1, "initialize callback failure throws");

    bool defaultThrew = false;
    try {
        static_cast<void>(ApplicationHeapDefault_nid_no_patch());
    } catch (const std::runtime_error&) {
        defaultThrew = true;
    }
    Require(defaultThrew, "ApplicationHeapDefault rethrows heapFailure instead of returning stale kind");

    bool allocateThrew = false;
    try {
        static_cast<void>(ApplicationHeapAllocate_nid_no_patch(16));
    } catch (const std::runtime_error&) {
        allocateThrew = true;
    }
    Require(allocateThrew, "allocation after init failure rethrows heapFailure");

    ApplicationHeapFinalize_nid_no_patch();
    Require(finalizeCalls == 0, "finalize after init failure does not run cleared finalize callback");
}

void CheckLifecycleAndRealign() {
    const void* process = BuildProcessParameters();
    ApplicationHeapInitialize_nid_no_patch(process);
    Require(initializeCalls == 1 && !ApplicationHeapDefault_nid_no_patch(), "initialize custom application heap");

    Require(ApplicationHeapRealign_nid_no_patch(nullptr, 0, 16) == heapStorage.data(), "ApplicationHeapRealign(nullptr, 0, 16) forwards to custom allocator");
    Require(realignCalls == 1 && realignLastPointer == nullptr && realignLastBytes == 0 && realignLastAlignment == 16, "ApplicationHeapRealign forwards expected arguments");

    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(nullptr, 0, 16) == heapStorage.data() && *__error_nid_postfix() == 77, "reallocalign(nullptr, 0, 16) forwards to custom allocator");
    Require(realignCalls == 2 && realignLastPointer == nullptr && realignLastBytes == 0 && realignLastAlignment == 16, "reallocalign(nullptr, 0, 16) forwards arguments");

    for (const std::size_t invalidAlignment : {std::size_t{0}, std::size_t{3}}) {
        *__error_nid_postfix() = 77;
        void* result = heapStorage.data();
        bool threw = false;
        try {
            result = reallocalign_nid_postfix(nullptr, 0, invalidAlignment);
        } catch (...) {
            threw = true;
        }
        Require(!threw && result == nullptr && *__error_nid_postfix() == 22, "reallocalign(nullptr, 0, invalid) returns null with EINVAL");
    }
    Require(realignCalls == 2, "reallocalign(nullptr, 0, invalid) does not call custom realign");

    realignReturnNull = true;
    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(nullptr, 0, 16) == nullptr && *__error_nid_postfix() == 12, "reallocalign(nullptr, 0, 16) OOM returns null with ENOMEM");
    Require(realignCalls == 3, "reallocalign(nullptr, 0, 16) OOM called custom realign");
    realignReturnNull = false;

    *__error_nid_postfix() = 77;
    Require(reallocalign_nid_postfix(heapStorage.data(), 0, 3) == nullptr && *__error_nid_postfix() == 77, "reallocalign(ptr, 0, 3) frees and returns null");
    Require(freeCalls == 1 && lastFreedPointer == heapStorage.data() && realignCalls == 3, "reallocalign(ptr, 0, 3) invokes free");
    lastFreedPointer = nullptr;
    Require(reallocalign_nid_postfix(heapStorage.data(), 0, 16) == nullptr && *__error_nid_postfix() == 77, "reallocalign(ptr, 0, 16) frees and returns null");
    Require(freeCalls == 2 && lastFreedPointer == heapStorage.data() && realignCalls == 3, "reallocalign(ptr, 0, 16) invokes free");

    ApplicationHeapFinalize_nid_no_patch();
    ApplicationHeapFinalize_nid_no_patch();
    Require(finalizeCalls == 1, "finalize invokes custom callback strictly once across recursive and repeat calls");

    bool postFinalizeThrew = false;
    try {
        static_cast<void>(ApplicationHeapAllocate_nid_no_patch(16));
    } catch (const std::runtime_error&) {
        postFinalizeThrew = true;
    }
    Require(postFinalizeThrew, "post-finalization allocation throws std::runtime_error");
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "init-failure") == 0) {
        CheckInitFailure();
        std::puts("application heap init-failure checks passed");
        return 0;
    }
    CheckLifecycleAndRealign();
    std::puts("application heap lifecycle checks passed");
    return 0;
}
