#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

extern "C" {
char* APS5_VABI __cxa_demangle_nid_postfix(const char*, char*, std::size_t*, int*);
void* APS5_VABI malloc_nid_postfix(std::size_t);
void APS5_VABI free_nid_postfix(void*);
}

namespace {

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
std::size_t failures = 0;
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

void Check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest demangle: %s\n", message);
    ++failures;
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

void RegisterAllocator() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&GuestAllocate);
    api[1] = reinterpret_cast<void*>(&GuestFree);
    api[2] = reinterpret_cast<void*>(&GuestCalloc);
    api[3] = reinterpret_cast<void*>(&GuestReallocate);
    api[4] = reinterpret_cast<void*>(&GuestAlignedAllocate);
    api[5] = reinterpret_cast<void*>(&GuestRealign);
    api[6] = reinterpret_cast<void*>(&GuestPosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void ReleaseIfReturned(void* result, void* original = nullptr) {
    ForgetIfMoved(original, result);
    free_nid_postfix(result != nullptr ? result : original);
}

void TestNullOutputBuffer() {
    std::size_t length = 0;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, &length, &status);
    Check(result != nullptr, "null output buffer did not demangle");
    Check(status == 0, "null output buffer returned the wrong status");
    Check(result != nullptr && std::strcmp(result, DemangledName) == 0, "null output buffer returned the wrong string");
    Check(length >= sizeof(DemangledName), "null output buffer returned an insufficient length");
    Check(result != nullptr && Owns(result), "null output buffer was not allocated by the guest allocator");
    ReleaseIfReturned(result);
}

void TestOptionalStatus() {
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, nullptr, nullptr);
    Check(result != nullptr && std::strcmp(result, DemangledName) == 0, "optional status output failed");
    Check(result != nullptr && Owns(result), "optional status output was not guest-owned");
    ReleaseIfReturned(result);
}

void TestLargeBufferReuse() {
    auto* buffer = static_cast<char*>(malloc_nid_postfix(128));
    const std::size_t originalLength = 128;
    std::size_t length = originalLength;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    Check(result == buffer, "sufficient guest buffer was not reused");
    Check(status == 0, "sufficient guest buffer returned the wrong status");
    Check(result != nullptr && std::strcmp(result, DemangledName) == 0, "sufficient guest buffer returned the wrong string");
    Check(length == originalLength, "sufficient guest buffer length changed");
    ReleaseIfReturned(result, buffer);
}

void TestSmallBufferGrowth() {
    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    std::size_t length = 1;
    const std::size_t reallocationsBefore = reallocationCalls;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    Check(result != nullptr, "too-small guest buffer did not grow");
    Check(status == 0, "grown guest buffer returned the wrong status");
    Check(result != nullptr && std::strcmp(result, DemangledName) == 0, "grown guest buffer returned the wrong string");
    Check(length >= sizeof(DemangledName), "grown guest buffer returned an insufficient length");
    Check(reallocationCalls == reallocationsBefore + 1, "too-small guest buffer bypassed guest reallocation");
    Check(result != nullptr && Owns(result), "grown buffer was not owned by the guest allocator");
    ReleaseIfReturned(result, buffer);
}

void TestInvalidInputs() {
    int status = 99;
    char* result = __cxa_demangle_nid_postfix("not_a_mangled_name", nullptr, nullptr, &status);
    Check(result == nullptr && status == -2, "invalid mangled name did not return status -2");
    status = 99;
    result = __cxa_demangle_nid_postfix(nullptr, nullptr, nullptr, &status);
    Check(result == nullptr && status == -3, "null mangled name did not return status -3");

    auto* buffer = static_cast<char*>(malloc_nid_postfix(32));
    std::memcpy(buffer, "unchanged", sizeof("unchanged"));
    result = __cxa_demangle_nid_postfix(MangledName, buffer, nullptr, &status);
    Check(result == nullptr && status == -3, "missing length for a supplied buffer did not return status -3");
    Check(std::strcmp(buffer, "unchanged") == 0, "invalid supplied-buffer arguments modified the buffer");
    free_nid_postfix(buffer);
}

void TestAllocationFailure() {
    const std::size_t failuresBefore = allocationFailures;
    failNextAllocation = true;
    std::size_t length = 0;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, nullptr, &length, &status);
    Check(result == nullptr, "guest allocation failure returned a buffer");
    Check(status == -1, "guest allocation failure did not return status -1");
    Check(allocationFailures == failuresBefore + 1, "demangler bypassed failed guest allocation");
    failNextAllocation = false;
    ReleaseIfReturned(result);
}

void TestReallocationFailure() {
    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    std::size_t length = 1;
    const std::size_t failuresBefore = reallocationFailures;
    failNextReallocation = true;
    int status = 99;
    char* result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, &status);
    Check(result == nullptr, "guest reallocation failure returned a buffer");
    Check(status == -1, "guest reallocation failure did not return status -1");
    Check(reallocationFailures == failuresBefore + 1, "demangler bypassed failed guest reallocation");
    Check(Owns(buffer), "failed guest reallocation did not preserve the input buffer");
    failNextReallocation = false;
    ReleaseIfReturned(result, buffer);
}

void TestAllocatorDiagnostics() {
    throwOnAllocation = true;
    bool allocationDiagnostic = false;
    char* result = nullptr;
    try {
        result = __cxa_demangle_nid_postfix(MangledName, nullptr, nullptr, nullptr);
    } catch (const std::runtime_error& error) {
        allocationDiagnostic = std::strcmp(error.what(), "guest allocator callback failure") == 0;
    }
    throwOnAllocation = false;
    Check(allocationDiagnostic, "allocator callback diagnostic was swallowed or changed");
    ReleaseIfReturned(result);

    auto* buffer = static_cast<char*>(malloc_nid_postfix(1));
    std::size_t length = 1;
    throwOnReallocation = true;
    bool reallocationDiagnostic = false;
    result = nullptr;
    try {
        result = __cxa_demangle_nid_postfix(MangledName, buffer, &length, nullptr);
    } catch (const std::runtime_error& error) {
        reallocationDiagnostic = std::strcmp(error.what(), "guest reallocator callback failure") == 0;
    }
    throwOnReallocation = false;
    Check(reallocationDiagnostic, "reallocator callback diagnostic was swallowed or changed");
    ReleaseIfReturned(result, buffer);
}

void Cleanup() {
    for (auto& allocation : allocations) {
        if (allocation.pointer != nullptr) std::free(allocation.pointer);
        allocation = {};
    }
}

}

int main() {
    RegisterAllocator();
    TestNullOutputBuffer();
    TestOptionalStatus();
    TestLargeBufferReuse();
    TestSmallBufferGrowth();
    TestInvalidInputs();
    TestAllocationFailure();
    TestReallocationFailure();
    TestAllocatorDiagnostics();
    Check(unexpectedFrees == 0, "guest free received a pointer outside its allocator");
    Check(unexpectedReallocations == 0, "guest reallocation received a pointer outside its allocator");
    Cleanup();
    return failures == 0 ? 0 : 1;
}
