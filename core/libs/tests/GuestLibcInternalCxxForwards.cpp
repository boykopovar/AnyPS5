#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>

namespace {

alignas(64) std::array<std::byte, 64> storage{};
unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal C++ runtime forwards: %s\n", message);
    std::abort();
}

void* APS5_VABI allocate(std::size_t) { return storage.data(); }

void APS5_VABI release(void* pointer) {
    ++releases;
    lastReleased = pointer;
}

void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI reallocate(void*, std::size_t) { return storage.data(); }
void* APS5_VABI align(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { return storage.data(); }
int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t) {
    *pointer = storage.data();
    return 0;
}

void Reset() {
    releases = 0;
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
    Reset();
    sceLibcInternalSizedNothrowDelete_nid_no_patch(storage.data(), 24, &nothrowTag);
    Require(releases == 1 && lastReleased == storage.data(), "the sized nothrow delete releases the block");

    Reset();
    sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(storage.data(), 64, nullptr);
    Require(releases == 1 && lastReleased == storage.data(), "the sized nothrow array delete releases the block");

    Reset();
    sceLibcInternalSizedNothrowDelete_nid_no_patch(nullptr, 24, &nothrowTag);
    sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(nullptr, 64, &nothrowTag);
    Require(releases == 0, "deleting null releases nothing");

    const std::error_category* const category = sceLibcInternalIostreamCategory_nid_no_patch();
    Require(category != nullptr, "the iostream category exists");
    Require(category == _ZSt17iostream_categoryv_nid_postfix(), "the iostream category is the libc object");
    Require(category == sceLibcInternalIostreamCategory_nid_no_patch(), "the iostream category is a singleton");
    Require(std::strcmp(category->name(), "iostream") == 0, "the category is named iostream");

    std::puts("libc internal C++ runtime forward checks passed");
    return 0;
}
