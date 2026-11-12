#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using FreeType::GuestBitmapLibrary;
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
    GuestBitmapLibrary library{};
    GuestSlotInternalRec internal{};
    GuestGlyphSlotRec slot{};

    Fixture() {
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        face.memory = &recorder.memory;
        library.memory = &recorder.memory;
        slot.library = &library;
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


void Ownership() {
    Fixture fixture;
    unsigned char pixels[12];
    for (unsigned i = 0; i < sizeof(pixels); ++i) pixels[i] = static_cast<unsigned char>(i + 1);
    int palette = 0;
    fixture.slot.format = FreeType::GlyphFormatBitmap;
    fixture.slot.bitmap = {3, 5, -4, pixels, 256, 2, 1, &palette};
    fixture.internal.flags = 0x40;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0, "Own_Bitmap copies a borrowed bitmap");
    const auto& bitmap = fixture.slot.bitmap;
    Require(fixture.recorder.allocs == 1 && fixture.recorder.lastSize == 12, "Own_Bitmap allocates |pitch| * rows bytes");
    Require(bitmap.buffer != pixels && std::memcmp(bitmap.buffer, pixels, sizeof(pixels)) == 0, "Own_Bitmap copies the pixels");
    Require(bitmap.rows == 3 && bitmap.width == 5 && bitmap.pitch == -4 && bitmap.num_grays == 256, "Own_Bitmap keeps the bitmap geometry");
    Require(bitmap.pixel_mode == 2 && bitmap.palette_mode == 1 && bitmap.palette == &palette, "Own_Bitmap keeps the pixel mode and palette");
    Require(fixture.internal.flags == (0x40 | FreeType::GlyphOwnBitmap), "Own_Bitmap marks the bitmap owned");
    unsigned char* owned = bitmap.buffer;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0 && bitmap.buffer == owned && fixture.recorder.allocs == 1, "Own_Bitmap leaves an owned bitmap alone");
    ft_glyphslot_free_bitmap_nid_postfix(&fixture.slot);
    Require(fixture.recorder.frees == 1 && fixture.recorder.lastFreed == owned, "free_bitmap releases the copy made by Own_Bitmap");
}

void OwnershipSkips() {
    Fixture fixture;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(nullptr) == 0, "Own_Bitmap of a null slot returns 0");
    unsigned char pixels[4]{};
    fixture.slot.bitmap = {1, 4, 4, pixels, 256, 2, 0, nullptr};
    fixture.slot.format = 0x6F75746C;
    fixture.slot.internal = nullptr;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0 && fixture.slot.bitmap.buffer == pixels, "Own_Bitmap ignores an outline slot");
    fixture.slot.internal = &fixture.internal;
    fixture.slot.format = FreeType::GlyphFormatBitmap;
    fixture.slot.bitmap = {7, 9, 2, nullptr, 256, 2, 0, nullptr};
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0, "Own_Bitmap of a null buffer succeeds");
    Require(!fixture.slot.bitmap.buffer && fixture.slot.bitmap.rows == 7 && fixture.recorder.allocs == 0, "Own_Bitmap of a null buffer allocates nothing");
    Require(fixture.internal.flags == FreeType::GlyphOwnBitmap, "Own_Bitmap of a null buffer marks the bitmap owned");
}

void OwnershipErrors() {
    Fixture fixture;
    unsigned char pixels[8]{};
    fixture.slot.format = FreeType::GlyphFormatBitmap;
    fixture.slot.bitmap = {2, 4, 4, pixels, 256, 2, 0, nullptr};
    fixture.recorder.fail = true;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0x40, "a failed Own_Bitmap returns Out_Of_Memory");
    Require(fixture.slot.bitmap.buffer == pixels && fixture.internal.flags == 0, "a failed Own_Bitmap leaves the slot unchanged");
    fixture.recorder.fail = false;
    fixture.slot.bitmap.rows = 0xFFFFFFFE;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0x06, "Own_Bitmap of a negative int size returns Invalid_Argument");
    Require(fixture.slot.bitmap.buffer == pixels && fixture.internal.flags == 0 && fixture.recorder.allocs == 1, "Own_Bitmap of a negative size leaves the slot unchanged");
    fixture.slot.bitmap.rows = 0;
    Require(sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(&fixture.slot) == 0, "Own_Bitmap of an empty bitmap succeeds");
    Require(!fixture.slot.bitmap.buffer && fixture.internal.flags == FreeType::GlyphOwnBitmap, "Own_Bitmap of an empty bitmap stores null and marks it owned");
}

}

int main() {
    Allocation();
    AllocationErrors();
    Release();
    Replacement();
    Ownership();
    OwnershipSkips();
    OwnershipErrors();
    std::puts("Guest FreeType Ot glyph slot tests passed");
    return 0;
}
