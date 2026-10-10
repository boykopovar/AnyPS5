#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
char* APS5_VABI __cxa_demangle_nid_postfix(const char*, char*, std::size_t*, int*);
void* APS5_VABI malloc_nid_postfix(std::size_t);
void APS5_VABI free_nid_postfix(void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrowsWithMessage;

constexpr char MangledName[] = "_ZN3foo3barEi";
constexpr char DemangledName[] = "foo::bar(int)";

struct Allocation {
    void* pointer = nullptr;
    std::size_t size = 0;
};

std::array<Allocation, 32> allocations{};
std::size_t allocationCalls = 0;
std::size_t allocationFailures = 0;
std::size_t reallocationCalls = 0;
std::size_t reallocationFailures = 0;
std::size_t unexpectedFrees = 0;
std::size_t unexpectedReallocations = 0;
bool failNextAllocation = false;
bool failNextReallocation = false;
bool throwOnAllocation = false;
bool throwOnReallocation = false;

std::size_t FindAllocation(const void* pointer) {
    for (std::size_t index = 0; index < allocations.size(); ++index) {
        if (allocations[index].pointer == pointer) return index;
    }
    return allocations.size();
}

bool Owns(const void* pointer) {
    return FindAllocation(pointer) != allocations.size();
}

void* Track(void* pointer, std::size_t size) {
    if (pointer == nullptr) return nullptr;
    for (auto& allocation : allocations) {
        if (allocation.pointer == nullptr) {
            allocation = {pointer, size};
            return pointer;
        }
    }
    std::free(pointer);
    return nullptr;
}

void Forget(const void* pointer) {
    const std::size_t index = FindAllocation(pointer);
    if (index != allocations.size()) allocations[index] = {};
}

void ForgetIfMoved(void* original, void* result) {
    if (original != nullptr && result != nullptr && result != original) Forget(original);
}

void* APS5_VABI GuestAllocate(std::size_t size) {
    ++allocationCalls;
    if (throwOnAllocation) throw std::runtime_error("guest allocator callback failure");
    if (failNextAllocation) {
        failNextAllocation = false;
        ++allocationFailures;
        return nullptr;
    }
    const std::size_t actualSize = size == 0 ? 1 : size;
    return Track(std::malloc(actualSize), actualSize);
}

void APS5_VABI GuestFree(void* pointer) {
    if (pointer == nullptr) return;
    const std::size_t index = FindAllocation(pointer);
    if (index == allocations.size()) {
        ++unexpectedFrees;
        return;
    }
    std::free(pointer);
    allocations[index] = {};
}

void* APS5_VABI GuestCalloc(std::size_t count, std::size_t size) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) return nullptr;
    const std::size_t total = count * size;
    void* pointer = GuestAllocate(total);
    if (pointer != nullptr) std::memset(pointer, 0, total);
    return pointer;
}

void* APS5_VABI GuestReallocate(void* pointer, std::size_t size) {
    ++reallocationCalls;
    if (throwOnReallocation) throw std::runtime_error("guest reallocator callback failure");
    if (failNextReallocation) {
        failNextReallocation = false;
        ++reallocationFailures;
        return nullptr;
    }
    if (pointer == nullptr) return GuestAllocate(size);
    if (size == 0) {
        GuestFree(pointer);
        return nullptr;
    }
    const std::size_t index = FindAllocation(pointer);
    if (index == allocations.size()) {
        ++unexpectedReallocations;
        return nullptr;
    }
    void* replacement = std::realloc(pointer, size);
    if (replacement != nullptr) allocations[index] = {replacement, size};
    return replacement;
}

void* APS5_VABI GuestAlignedAllocate(std::size_t, std::size_t size) {
    return GuestAllocate(size);
}

void* APS5_VABI GuestRealign(void* pointer, std::size_t size, std::size_t) {
    return GuestReallocate(pointer, size);
}

int APS5_VABI GuestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    if (pointer == nullptr) return 22;
    *pointer = GuestAllocate(size);
    return *pointer == nullptr ? 12 : 0;
}

void ResetAllocator() {
    for (auto& allocation : allocations) {
        if (allocation.pointer != nullptr) std::free(allocation.pointer);
        allocation = {};
    }
    allocationCalls = 0;
    allocationFailures = 0;
    reallocationCalls = 0;
    reallocationFailures = 0;
    unexpectedFrees = 0;
    unexpectedReallocations = 0;
    failNextAllocation = false;
    failNextReallocation = false;
    throwOnAllocation = false;
    throwOnReallocation = false;
}

class AllocatorFixture {
public:
    AllocatorFixture() {
        std::array<void*, 10> api{};
        api[0] = reinterpret_cast<void*>(&GuestAllocate);
        api[1] = reinterpret_cast<void*>(&GuestFree);
        api[2] = reinterpret_cast<void*>(&GuestCalloc);
        api[3] = reinterpret_cast<void*>(&GuestReallocate);
        api[4] = reinterpret_cast<void*>(&GuestAlignedAllocate);
        api[5] = reinterpret_cast<void*>(&GuestRealign);
        api[6] = reinterpret_cast<void*>(&GuestPosixAlign);
        ApplicationHeapRegister_nid_no_patch(api.data());
        ResetAllocator();
    }

    ~AllocatorFixture() {
        ResetAllocator();
    }

    AllocatorFixture(const AllocatorFixture&) = delete;
    AllocatorFixture& operator=(const AllocatorFixture&) = delete;
};

void ReleaseIfReturned(void* result, void* original = nullptr) {
    ForgetIfMoved(original, result);
    free_nid_postfix(result != nullptr ? result : original);
}

void RequireAllocatorConsistent() {
    RequireEqual(unexpectedFrees, std::size_t{0}, "guest free received a pointer outside its allocator");
    RequireEqual(unexpectedReallocations, std::size_t{0}, "guest reallocation received a pointer outside its allocator");
}

const Case nullOutputBuffer{"Demangle_NullOutputBuffer_AllocatesResultFromGuestHeap", [] {
    const AllocatorFixture fixture;
    std::size_t length = 0;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, &length, &status);
    Require(result != nullptr, "null output buffer did not demangle");
    RequireEqual(status, 0, "null output buffer status");
    RequireEqual(std::string_view(result), std::string_view(DemangledName), "null output buffer string");
    Require(length >= sizeof(DemangledName), "null output buffer returned an insufficient length");
    Require(Owns(result), "null output buffer was not allocated by the guest allocator");
    ReleaseIfReturned(result);
    RequireAllocatorConsistent();
}};

const Case optionalStatus{"Demangle_NullLengthAndStatus_StillDemangles", [] {
    const AllocatorFixture fixture;
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, nullptr, nullptr);
    Require(result != nullptr, "optional status output failed");
    RequireEqual(std::string_view(result), std::string_view(DemangledName), "optional status output string");
    Require(Owns(result), "optional status output was not guest-owned");
    ReleaseIfReturned(result);
    RequireAllocatorConsistent();
}};

const Case largeBuffer{"Demangle_SufficientGuestBuffer_IsReused", [] {
    const AllocatorFixture fixture;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(128));
    const std::size_t originalLength = 128;
    std::size_t length = originalLength;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    Require(result == buffer, "sufficient guest buffer was not reused");
    RequireEqual(status, 0, "sufficient guest buffer status");
    RequireEqual(std::string_view(result), std::string_view(DemangledName), "sufficient guest buffer string");
    RequireEqual(length, originalLength, "sufficient guest buffer length");
    ReleaseIfReturned(result, buffer);
    RequireAllocatorConsistent();
}};

const Case smallBuffer{"Demangle_TooSmallGuestBuffer_GrowsThroughGuestReallocation", [] {
    const AllocatorFixture fixture;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    std::size_t length = 1;
    const std::size_t reallocationsBefore = reallocationCalls;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    Require(result != nullptr, "too-small guest buffer did not grow");
    RequireEqual(status, 0, "grown guest buffer status");
    RequireEqual(std::string_view(result), std::string_view(DemangledName), "grown guest buffer string");
    Require(length >= sizeof(DemangledName), "grown guest buffer returned an insufficient length");
    RequireEqual(reallocationCalls, reallocationsBefore + 1, "too-small guest buffer guest reallocations");
    Require(Owns(result), "grown buffer was not owned by the guest allocator");
    ReleaseIfReturned(result, buffer);
    RequireAllocatorConsistent();
}};

const Case invalidName{"Demangle_InvalidMangledName_ReturnsStatusMinusTwo", [] {
    const AllocatorFixture fixture;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix("not_a_mangled_name", nullptr, nullptr, &status);
    Require(result == nullptr, "invalid mangled name returned a buffer");
    RequireEqual(status, -2, "invalid mangled name status");
    RequireAllocatorConsistent();
}};

const Case nullName{"Demangle_NullMangledName_ReturnsStatusMinusThree", [] {
    const AllocatorFixture fixture;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(nullptr, nullptr, nullptr, &status);
    Require(result == nullptr, "null mangled name returned a buffer");
    RequireEqual(status, -3, "null mangled name status");
    RequireAllocatorConsistent();
}};

const Case bufferWithoutLength{"Demangle_BufferWithoutLength_ReturnsStatusMinusThreeAndKeepsBuffer", [] {
    const AllocatorFixture fixture;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(32));
    std::memcpy(buffer, "unchanged", sizeof("unchanged"));
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, nullptr, &status);
    const std::string contents(buffer);
    free_nid_postfix(buffer);
    Require(result == nullptr, "missing length for a supplied buffer returned a buffer");
    RequireEqual(status, -3, "missing length for a supplied buffer status");
    RequireEqual(contents, std::string("unchanged"), "invalid supplied-buffer arguments modified the buffer");
    RequireAllocatorConsistent();
}};

const Case allocationFailure{"Demangle_GuestAllocationFails_ReturnsStatusMinusOne", [] {
    const AllocatorFixture fixture;
    const std::size_t failuresBefore = allocationFailures;
    failNextAllocation = true;
    std::size_t length = 0;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, &length, &status);
    failNextAllocation = false;
    ReleaseIfReturned(result);
    Require(result == nullptr, "guest allocation failure returned a buffer");
    RequireEqual(status, -1, "guest allocation failure status");
    RequireEqual(allocationFailures, failuresBefore + 1, "demangler used the failed guest allocation");
    RequireAllocatorConsistent();
}};

const Case reallocationFailure{"Demangle_GuestReallocationFails_ReturnsStatusMinusOneAndKeepsBuffer", [] {
    const AllocatorFixture fixture;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    std::size_t length = 1;
    const std::size_t failuresBefore = reallocationFailures;
    failNextReallocation = true;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    failNextReallocation = false;
    Require(result == nullptr, "guest reallocation failure returned a buffer");
    RequireEqual(status, -1, "guest reallocation failure status");
    RequireEqual(reallocationFailures, failuresBefore + 1, "demangler used the failed guest reallocation");
    Require(Owns(buffer), "failed guest reallocation did not preserve the input buffer");
    ReleaseIfReturned(result, buffer);
    RequireAllocatorConsistent();
}};

const Case allocatorThrows{"Demangle_GuestAllocatorThrows_PropagatesDiagnostic", [] {
    const AllocatorFixture fixture;
    throwOnAllocation = true;
    RequireThrowsWithMessage<std::runtime_error>([] { __cxa_demangle_nid_postfix(MangledName, nullptr, nullptr, nullptr); },
        "guest allocator callback failure", "allocator callback diagnostic");
    throwOnAllocation = false;
    RequireAllocatorConsistent();
}};

const Case reallocatorThrows{"Demangle_GuestReallocatorThrows_PropagatesDiagnostic", [] {
    const AllocatorFixture fixture;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    throwOnReallocation = true;
    RequireThrowsWithMessage<std::runtime_error>([buffer] {
        std::size_t length = 1;
        __cxa_demangle_nid_postfix(MangledName, buffer, &length, nullptr);
    }, "guest reallocator callback failure", "reallocator callback diagnostic");
    throwOnReallocation = false;
    ReleaseIfReturned(nullptr, buffer);
    RequireAllocatorConsistent();
}};

} // namespace
