#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

alignas(256) std::array<std::byte, 64> storage{};
unsigned failuresLeft = 0;
unsigned allocations = 0;
unsigned releases = 0;
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal aligned new: %s\n", message);
    std::abort();
}

void* APS5_VABI allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    if (failuresLeft != 0) {
        --failuresLeft;
        return nullptr;
    }
    return storage.data();
}

void APS5_VABI release(void* pointer) {
    ++releases;
    lastReleased = pointer;
}

void* APS5_VABI allocateZeroed(std::size_t count, std::size_t bytes) { return allocate(count * bytes); }
void* APS5_VABI reallocate(void*, std::size_t bytes) { return allocate(bytes); }
void* APS5_VABI align(std::size_t alignment, std::size_t bytes) {
    lastAlignment = alignment;
    return allocate(bytes);
}
void* APS5_VABI realign(void*, std::size_t bytes, std::size_t) { return allocate(bytes); }
int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = allocate(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void Reset(unsigned failures) {
    failuresLeft = failures;
    allocations = 0;
    releases = 0;
    lastSize = 0;
    lastAlignment = 0;
    lastReleased = nullptr;
}

}

int main() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&allocate);
    api[1] = reinterpret_cast<void*>(&release);
    api[2] = reinterpret_cast<void*>(&allocateZeroed);
    api[3] = reinterpret_cast<void*>(&reallocate);
    api[4] = reinterpret_cast<void*>(&align);
    api[5] = reinterpret_cast<void*>(&realign);
    api[6] = reinterpret_cast<void*>(&posixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());

    const int nothrowTag = 0;
    Reset(0);
    void* array = sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(56, 256, &nothrowTag);
    Require(array == storage.data(), "the array comes from the application heap");
    Require(lastSize == 56 && lastAlignment == 256 && allocations == 1, "size and alignment reach the aligned allocator");

    sceLibcInternalAlignedArrayDelete_nid_no_patch(array, 256);
    Require(releases == 1 && lastReleased == storage.data(), "the aligned delete releases the array");

    Reset(0);
    Require(sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(0, 32, nullptr) == storage.data() && lastSize == 1 && lastAlignment == 32, "a zero-size array asks for one byte");

    Reset(1);
    Require(sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(8, 64, &nothrowTag) == nullptr && allocations == 1, "a failed allocation returns null without throwing");

    Reset(2);
    Require(sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(8, 64, &nothrowTag) == nullptr && allocations == 1, "without a new handler the first failure returns null");

    Reset(0);
    sceLibcInternalAlignedArrayDelete_nid_no_patch(nullptr, 64);
    Require(releases == 0, "deleting null releases nothing");

    std::puts("libc internal aligned array new checks passed");
    return 0;
}
