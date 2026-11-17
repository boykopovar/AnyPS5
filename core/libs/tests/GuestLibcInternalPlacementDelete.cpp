#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

unsigned releases = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal placement delete: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

bool Untouched(const unsigned char* bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (bytes[index] != 0x5A) return false;
    }
    return true;
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate),
        reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc),
        reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign),
        reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api.data());

    auto* block = static_cast<unsigned char*>(_Znwm_nid_postfix(32));
    std::memset(block, 0x5A, 32);
    releases = 0;

    sceLibcInternalPlacementDelete_nid_no_patch(block, block);
    sceLibcInternalPlacementArrayDelete_nid_no_patch(block, block);
    sceLibcInternalPlacementDelete_nid_no_patch(nullptr, block);
    sceLibcInternalPlacementArrayDelete_nid_no_patch(nullptr, block);
    Require(releases == 0, "the placement forms free nothing");
    Require(Untouched(block, 32), "the placement forms leave the block unchanged");

    _ZdlPv_nid_postfix(block);
    Require(releases == 1, "the block is still owned by the application heap");

    std::puts("libc internal placement delete checks passed");
    return 0;
}
