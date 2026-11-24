#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceJemspace/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>


namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void CheckBytes(const void* allocation, std::size_t bytes, unsigned char value) {
    const auto* data = static_cast<const unsigned char*>(allocation);
    for (std::size_t i = 0; i < bytes; ++i) {
        Require(data[i] == value, "Allocation contents must be preserved");
    }
}

void CheckGuestCopies(void* allocation) {
    std::array<unsigned char, 64> saved{};
    Require(GuestCopyIn_nid_no_patch(saved.data(), allocation, saved.size()),
            "GuestCopyIn must read an allocated payload");
    std::array<unsigned char, 64> source;
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<unsigned char>(0x10 + i);
    }
    Require(GuestCopyOut_nid_no_patch(allocation, source.data(), source.size()),
            "GuestCopyOut must write an allocated payload");
    const auto* written = static_cast<const unsigned char*>(allocation);
    for (std::size_t i = 0; i < source.size(); ++i) {
        Require(written[i] == source[i], "GuestCopyOut must write the varied pattern");
    }
    source.fill(0);
    Require(GuestCopyIn_nid_no_patch(source.data(), allocation, source.size()),
            "GuestCopyIn must read back an allocated payload");
    for (std::size_t i = 0; i < source.size(); ++i) {
        Require(source[i] == 0x10 + i, "GuestCopyIn must return the varied pattern");
    }
    Require(GuestCopyOut_nid_no_patch(allocation, saved.data(), saved.size()),
            "GuestCopyOut must restore the saved payload");
}

void APS5_VABI CaptureStats(void* opaque, const char* text) {
    static_cast<std::string*>(opaque)->append(text);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "The default guest heap must be selected");

    void* space = sceJemspaceCreate("default", 64 * 1024 * 1024, 0);
    Require(space != nullptr, "Create must return a default-heap arena");

    void* block = sceJemspaceMalloc(space, 64);
    Require(block != nullptr, "Malloc must allocate on the default guest heap");
    std::memset(block, 0x3c, 64);
    CheckGuestCopies(block);
    Require(sceJemspaceMallocUsableSize(block) >= 64, "Usable size must cover the requested size");

    void* zeroed = sceJemspaceCalloc(space, 8, 32);
    Require(zeroed != nullptr, "Calloc must allocate on the default guest heap");
    CheckBytes(zeroed, 256, 0);

    void* aligned = sceJemspaceMemalign(space, 8193, 128);
    Require(aligned != nullptr, "Memalign must allocate on the default guest heap");
    Require(reinterpret_cast<std::uintptr_t>(aligned) % 16384 == 0,
            "Memalign must honor the native flag encoding of alignment 8193 as effective alignment 16384");
    std::memset(aligned, 0x5d, 128);

    void* posix = nullptr;
    Require(sceJemspacePosixMemalign(space, &posix, 64, 256) == 0 && posix != nullptr,
            "PosixMemalign must allocate on the default guest heap");
    Require(reinterpret_cast<std::uintptr_t>(posix) % 2 == 0,
            "PosixMemalign must honor the native flag encoding of alignment 64 as effective alignment 2");
    std::memset(posix, 0x6e, 256);

    block = sceJemspaceRealloc(space, block, 512);
    Require(block != nullptr, "Realloc must grow an allocation on the default guest heap");
    CheckBytes(block, 64, 0x3c);
    std::memset(static_cast<unsigned char*>(block) + 64, 0x3c, 512 - 64);
    CheckGuestCopies(block);

    block = sceJemspaceReallocalign(space, block, 32769, 1024);
    Require(block != nullptr, "Reallocalign must realign an allocation on the default guest heap");
    Require(reinterpret_cast<std::uintptr_t>(block) % 65536 == 0,
            "Reallocalign must honor the native flag encoding of alignment 32769 as effective alignment 65536");
    CheckBytes(block, 64, 0x3c);

    std::array<std::uint64_t, 5> stats{};
    sceJemspaceGetStats(space, stats.data());
    Require(stats[0] == 64 * 1024 * 1024, "Statistics must retain the configured arena limit");
    Require(stats[1] <= stats[0], "Committed bytes must not exceed the arena limit");
    Require(stats[2] >= stats[1], "Reserved bytes must cover committed bytes");
    Require(stats[3] >= stats[1], "Peak committed bytes must cover committed bytes");
    Require(stats[4] >= stats[2], "Peak reserved bytes must cover reserved bytes");

    std::string output;
    sceJemspacePrintStats(CaptureStats, &output, nullptr);
    Require(!output.empty(), "PrintStats must produce a report through the guest callback");

    Require(sceJemspacePurge(space) == 0, "Purge must accept a live default-heap arena");
    CheckBytes(block, 64, 0x3c);

    Require(sceJemspaceFree(space, block) == 0, "Free must release a realigned allocation");
    Require(sceJemspaceFree(space, zeroed) == 0, "Free must release a calloc allocation");
    Require(sceJemspaceFree(space, aligned) == 0, "Free must release a memalign allocation");
    Require(sceJemspaceFree(space, posix) == 0, "Free must release a posix-memalign allocation");

    Require(sceJemspaceDestroy(space) == 0, "Destroy must release the default-heap arena");
}
