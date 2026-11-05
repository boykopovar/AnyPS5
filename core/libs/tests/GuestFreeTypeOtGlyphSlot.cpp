#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using FreeType::GuestFaceRec;
using FreeType::GuestGlyphSlotRec;
using FreeType::GuestMemoryRec;
using FreeType::GuestSlotInternalRec;

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
    std::int64_t lastSize = 0;
    void* lastFreed = nullptr;
    bool fail = false;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastSize = size;
    if (recorder->fail) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->frees;
    recorder->lastFreed = block;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

struct Fixture {
    Recorder recorder;
    GuestFaceRec face{};
    GuestSlotInternalRec internal{};
    GuestGlyphSlotRec slot{};

    Fixture() {
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        face.memory = &recorder.memory;
        slot.face = &face;
        slot.internal = &internal;
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot glyph slot: %s\n", message);
    std::exit(1);
}

bool Zeroed(const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

void Allocation() {
    Fixture fixture;
    fixture.internal.flags = 0x10;
    unsigned char borrowed[4]{};
    fixture.slot.bitmap.buffer = borrowed;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 12) == 0, "alloc_bitmap succeeds");
    Require(fixture.internal.flags == (0x10 | FreeType::GlyphOwnBitmap), "alloc_bitmap marks the bitmap owned and keeps other flags");
    Require(fixture.recorder.allocs == 1 && fixture.recorder.lastSize == 12 && fixture.recorder.frees == 0, "alloc_bitmap does not free a borrowed buffer");
    Require(fixture.slot.bitmap.buffer != borrowed && Zeroed(fixture.slot.bitmap.buffer, 12), "alloc_bitmap stores a cleared buffer");
    unsigned char* first = fixture.slot.bitmap.buffer;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 5) == 0, "a second alloc_bitmap succeeds");
    Require(fixture.recorder.frees == 1 && fixture.recorder.lastFreed == first, "a second alloc_bitmap frees the owned buffer");
    Require(fixture.recorder.allocs == 2 && Zeroed(fixture.slot.bitmap.buffer, 5), "a second alloc_bitmap stores a new cleared buffer");
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 0) == 0, "alloc_bitmap of 0 bytes succeeds");
    Require(fixture.recorder.frees == 2 && fixture.recorder.allocs == 2 && !fixture.slot.bitmap.buffer, "alloc_bitmap of 0 bytes frees and stores null");
    Require((fixture.internal.flags & FreeType::GlyphOwnBitmap) != 0, "alloc_bitmap of 0 bytes keeps the owned flag");
}

void AllocationErrors() {
    Fixture fixture;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, ~std::uint64_t{0}) == 0x06, "alloc_bitmap of a negative FT_Long size returns Invalid_Argument");
    Require(!fixture.slot.bitmap.buffer && fixture.recorder.allocs == 0, "alloc_bitmap of a negative size allocates nothing");
    Require(fixture.internal.flags == FreeType::GlyphOwnBitmap, "alloc_bitmap of a negative size still marks the bitmap owned");
    fixture.recorder.fail = true;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 8) == 0x40, "a failed alloc_bitmap returns Out_Of_Memory");
    Require(!fixture.slot.bitmap.buffer && fixture.recorder.allocs == 1 && fixture.recorder.frees == 0, "a failed alloc_bitmap stores null");
    Fixture nullMemory;
    nullMemory.face.memory = nullptr;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&nullMemory.slot, 0) == 0, "alloc_bitmap of 0 bytes does not read a null memory");
    Require(!nullMemory.slot.bitmap.buffer && nullMemory.internal.flags == FreeType::GlyphOwnBitmap, "alloc_bitmap of 0 bytes with a null memory stores null");
}

void Release() {
    Fixture fixture;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 3) == 0, "alloc_bitmap before free_bitmap succeeds");
    unsigned char* owned = fixture.slot.bitmap.buffer;
    fixture.internal.flags |= 0x20;
    ft_glyphslot_free_bitmap_nid_postfix(&fixture.slot);
    Require(fixture.recorder.frees == 1 && fixture.recorder.lastFreed == owned, "free_bitmap frees an owned buffer");
    Require(!fixture.slot.bitmap.buffer && fixture.internal.flags == 0x20, "free_bitmap clears the buffer and only the owned flag");
    unsigned char borrowed[2]{};
    fixture.slot.bitmap.buffer = borrowed;
    ft_glyphslot_free_bitmap_nid_postfix(&fixture.slot);
    Require(fixture.recorder.frees == 1 && !fixture.slot.bitmap.buffer, "free_bitmap drops a borrowed buffer without freeing it");
    Fixture noInternal;
    noInternal.slot.internal = nullptr;
    noInternal.slot.face = nullptr;
    noInternal.slot.bitmap.buffer = borrowed;
    ft_glyphslot_free_bitmap_nid_postfix(&noInternal.slot);
    Require(!noInternal.slot.bitmap.buffer && noInternal.recorder.frees == 0, "free_bitmap without internal data drops the buffer");
}

void Replacement() {
    Fixture fixture;
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(&fixture.slot, 6) == 0, "alloc_bitmap before set_bitmap succeeds");
    unsigned char* owned = fixture.slot.bitmap.buffer;
    unsigned char external[6]{};
    ft_glyphslot_set_bitmap_nid_postfix(&fixture.slot, external);
    Require(fixture.recorder.frees == 1 && fixture.recorder.lastFreed == owned, "set_bitmap frees the owned buffer");
    Require(fixture.slot.bitmap.buffer == external && fixture.internal.flags == 0, "set_bitmap stores a borrowed buffer");
    unsigned char other[1]{};
    ft_glyphslot_set_bitmap_nid_postfix(&fixture.slot, other);
    Require(fixture.recorder.frees == 1 && fixture.slot.bitmap.buffer == other, "set_bitmap replaces a borrowed buffer without freeing it");
    ft_glyphslot_set_bitmap_nid_postfix(&fixture.slot, nullptr);
    Require(!fixture.slot.bitmap.buffer && fixture.internal.flags == 0, "set_bitmap accepts a null buffer");
}

}

int main() {
    Allocation();
    AllocationErrors();
    Release();
    Replacement();
    std::puts("Guest FreeType Ot glyph slot tests passed");
    return 0;
}
