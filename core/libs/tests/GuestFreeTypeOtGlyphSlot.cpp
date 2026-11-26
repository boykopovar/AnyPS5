#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using FreeType::GuestBitmapLibrary;
using FreeType::GuestDriverClassPrefix;
using FreeType::GuestDriverRec;
using FreeType::GuestGlyphLoaderRec;
using FreeType::GuestModuleClass;
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
    int failAt = 0;
    GuestGlyphSlotRec* rejectedSlot = nullptr;
    bool internalClearedBeforeSlotFree = false;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastSize = size;
    if (recorder->fail || recorder->allocs == recorder->failAt) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->frees;
    recorder->lastFreed = block;
    if (block == recorder->rejectedSlot) recorder->internalClearedBeforeSlotFree = recorder->rejectedSlot->internal == nullptr;
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

struct SlotCalls {
    int inits = 0;
    int dones = 0;
    GuestGlyphSlotRec* initSlot = nullptr;
    GuestGlyphSlotRec* doneSlot = nullptr;
    int initResult = 0;
    bool fillOnInit = false;
};

SlotCalls calls;

int APS5_VABI RecordInitSlot(GuestGlyphSlotRec* slot) {
    ++calls.inits;
    calls.initSlot = slot;
    if (calls.fillOnInit) {
        auto* memory = static_cast<GuestFaceRec*>(slot->face)->memory;
        auto* recorder = Owner(static_cast<GuestMemoryRec*>(memory));
        auto* loader = static_cast<GuestGlyphLoaderRec*>(static_cast<GuestSlotInternalRec*>(slot->internal)->loader);
        loader->base.outline.points = static_cast<FreeType::GuestVector*>(recorder->memory.alloc(&recorder->memory, 16));
        loader->base.subglyphs = recorder->memory.alloc(&recorder->memory, 8);
        Require(ft_glyphslot_alloc_bitmap_nid_postfix(slot, 4) == 0, "init_slot allocates an owned bitmap");
    }
    return calls.initResult;
}

void APS5_VABI RecordDoneSlot(GuestGlyphSlotRec* slot) {
    ++calls.dones;
    calls.doneSlot = slot;
    auto* face = static_cast<GuestFaceRec*>(slot->face);
    Owner(static_cast<GuestMemoryRec*>(face->memory))->rejectedSlot = slot;
}

GuestGlyphSlotRec* gCorruptTargetSlot = nullptr;
void* gSavedInternal = nullptr;
void* gSavedLoader = nullptr;

GuestGlyphSlotRec* gInsertedHead = nullptr;
GuestGlyphSlotRec* gInsertedTail = nullptr;

int APS5_VABI InsertHeadOnInit(GuestGlyphSlotRec* slot) {
    if (gInsertedHead && slot && slot->face) {
        auto* face = static_cast<GuestFaceRec*>(slot->face);
        gInsertedHead->next = gInsertedTail;
        face->glyph = gInsertedHead;
    }
    return 0;
}

int APS5_VABI CorruptFaceOnInit(GuestGlyphSlotRec* slot) {
    gCorruptTargetSlot = slot;
    gSavedInternal = slot->internal;
    gSavedLoader = gSavedInternal ? static_cast<GuestSlotInternalRec*>(gSavedInternal)->loader : nullptr;
    slot->face = reinterpret_cast<GuestFaceRec*>(0x10);
    return 0x24;
}

struct DriverFixture {
    Recorder recorder;
    GuestFaceRec face{};
    GuestModuleClass moduleClass{};
    GuestDriverClassPrefix clazz{};
    GuestDriverRec driver{};
    int library = 0;

    DriverFixture() {
        calls = {};
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        face.memory = &recorder.memory;
        face.driver = &driver;
        clazz.slot_object_size = sizeof(GuestGlyphSlotRec) + 16;
        clazz.init_slot = RecordInitSlot;
        clazz.done_slot = RecordDoneSlot;
        driver.root_clazz = &moduleClass;
        driver.root_library = &library;
        driver.root_memory = &recorder.memory;
        driver.clazz = &clazz;
    }

    DriverFixture(const DriverFixture&) = delete;
    DriverFixture& operator=(const DriverFixture&) = delete;

    void Release(GuestGlyphSlotRec* slot) {
        auto* internal = static_cast<GuestSlotInternalRec*>(slot->internal);
        std::free(internal->loader);
        std::free(internal);
        std::free(slot);
    }
};

void NewSlot() {
    DriverFixture fixture;
    GuestGlyphSlotRec previous{};
    fixture.face.glyph = &previous;
    GuestGlyphSlotRec* slot = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0, "New_GlyphSlot succeeds");
    Require(slot && fixture.face.glyph == slot && slot->next == &previous, "New_GlyphSlot links the slot at the head of the face list");
    Require(slot->face == &fixture.face && slot->library == &fixture.library, "New_GlyphSlot sets the face and the driver library");
    Require(fixture.recorder.allocs == 3 && fixture.recorder.frees == 0, "New_GlyphSlot allocates the slot, internal record and glyph loader");
    auto* internal = static_cast<GuestSlotInternalRec*>(slot->internal);
    auto* loader = static_cast<GuestGlyphLoaderRec*>(internal->loader);
    Require(internal->flags == 0 && loader && loader->memory == &fixture.recorder.memory, "New_GlyphSlot creates a glyph loader on the driver memory");
    Require(Zeroed(reinterpret_cast<const unsigned char*>(loader) + 8, sizeof(GuestGlyphLoaderRec) - 8), "New_GlyphSlot clears the glyph loader");
    Require(Zeroed(reinterpret_cast<const unsigned char*>(slot) + sizeof(GuestGlyphSlotRec), 16), "New_GlyphSlot clears slot_object_size bytes");
    Require(calls.inits == 1 && calls.initSlot == slot && calls.dones == 0, "New_GlyphSlot calls init_slot once");
    fixture.Release(slot);
}

void NewSlotWithoutOutlines() {
    DriverFixture fixture;
    fixture.moduleClass.module_flags = FreeType::ModuleDriverNoOutlines;
    fixture.clazz.init_slot = nullptr;
    GuestGlyphSlotRec* slot = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0, "New_GlyphSlot without outlines succeeds");
    Require(fixture.recorder.allocs == 2 && !static_cast<GuestSlotInternalRec*>(slot->internal)->loader, "New_GlyphSlot without outlines creates no glyph loader");
    Require(fixture.face.glyph == slot && !slot->next && calls.inits == 0, "New_GlyphSlot without init_slot links the slot");
    fixture.Release(slot);
    DriverFixture unnamed;
    Require(FT_New_GlyphSlot_nid_postfix(&unnamed.face, nullptr) == 0 && unnamed.face.glyph, "New_GlyphSlot with a null slot pointer links the slot");
    unnamed.Release(unnamed.face.glyph);
}

void NewSlotArguments() {
    GuestGlyphSlotRec sentinel{};
    GuestGlyphSlotRec* slot = &sentinel;
    Require(FT_New_GlyphSlot_nid_postfix(nullptr, &slot) == 0x06 && slot == &sentinel, "New_GlyphSlot of a null face returns Invalid_Argument");
    DriverFixture fixture;
    fixture.face.driver = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0x06 && slot == &sentinel, "New_GlyphSlot of a face without driver returns Invalid_Argument");
    fixture.face.driver = &fixture.driver;
    fixture.clazz.slot_object_size = -1;
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0x06, "New_GlyphSlot of a negative slot size returns Invalid_Argument");
    Require(!slot && fixture.recorder.allocs == 0 && !fixture.face.glyph, "New_GlyphSlot of a negative slot size clears the slot pointer");
}

void NewSlotFailures() {
    DriverFixture fixture;
    GuestGlyphSlotRec sentinel{};
    GuestGlyphSlotRec* slot = &sentinel;
    fixture.recorder.failAt = 1;
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0x40 && !slot, "a failed slot allocation returns Out_Of_Memory and clears the slot pointer");
    Require(calls.dones == 0 && fixture.recorder.frees == 0 && !fixture.face.glyph, "a failed slot allocation calls nothing");
    for (int failAt = 2; failAt <= 3; ++failAt) {
        DriverFixture partial;
        partial.recorder.failAt = failAt;
        slot = &sentinel;
        errno = 71;
        Require(FT_New_GlyphSlot_nid_postfix(&partial.face, &slot) == 0x40 && slot == &sentinel, "a failed slot initialisation returns Out_Of_Memory");
        Require(calls.inits == 0 && calls.dones == 1 && partial.recorder.frees == failAt - 1, "a failed slot initialisation calls done_slot and frees what it allocated");
        Require(!partial.face.glyph, "a failed slot initialisation does not link the slot");
        Require(partial.recorder.internalClearedBeforeSlotFree, "failed initialisation clears slot->internal before the allocator frees the slot");
        Require(errno == 71, "failed initialisation preserves errno while clearing slot->internal");
    }
    DriverFixture rejected;
    calls.initResult = 0x24;
    calls.fillOnInit = true;
    slot = &sentinel;
    errno = 71;
    Require(FT_New_GlyphSlot_nid_postfix(&rejected.face, &slot) == 0x24 && slot == &sentinel, "New_GlyphSlot returns the init_slot error");
    Require(calls.inits == 1 && calls.dones == 1 && calls.doneSlot == calls.initSlot, "a rejected slot is passed to done_slot");
    Require(rejected.recorder.allocs == 6 && rejected.recorder.frees == 6 && !rejected.face.glyph, "a rejected slot frees its bitmap, loader arrays, loader, internal record and slot");
    Require(rejected.recorder.lastFreed == calls.initSlot, "a rejected slot is freed last");
    Require(rejected.recorder.internalClearedBeforeSlotFree, "init_slot failure clears slot->internal before the allocator frees the slot");
    Require(errno == 71, "init_slot failure preserves errno while clearing slot->internal");
}

void GuestPointers() {
    DriverFixture fixture;
    GuestGlyphSlotRec* slot = reinterpret_cast<GuestGlyphSlotRec*>(0x1234);
    static Recorder roRecorder{};
    roRecorder.memory = {&roRecorder, RecordAlloc, RecordFree, RecordRealloc};
    static GuestModuleClass roModuleClass{};
    static GuestDriverClassPrefix roClazz{.slot_object_size = sizeof(GuestGlyphSlotRec)};
    static int roLibrary = 0;
    static GuestDriverRec roDriver{.root_clazz = &roModuleClass, .root_library = &roLibrary, .root_memory = &roRecorder.memory, .clazz = &roClazz};
    static GuestSlotInternalRec roInternal{};
    static const GuestFaceRec readOnlyFace{.driver = &roDriver, .memory = &roRecorder.memory};
    static const GuestGlyphSlotRec readOnlySlot{.face = const_cast<GuestFaceRec*>(&readOnlyFace), .internal = &roInternal};
    unsigned char buffer[4]{};

    errno = 71;
    Require(FT_New_GlyphSlot_nid_postfix(reinterpret_cast<GuestFaceRec*>(0x10), &slot) == 0x06 && errno == 71 && slot == reinterpret_cast<GuestGlyphSlotRec*>(0x1234),
        "New_GlyphSlot refuses an unreadable face without changing errno or output");
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, reinterpret_cast<GuestGlyphSlotRec**>(0x10)) == 0x06 && errno == 71 && !fixture.face.glyph && fixture.recorder.allocs == fixture.recorder.frees,
        "New_GlyphSlot rolls back when the output slot pointer is unwritable");

    DriverFixture listFixture;
    GuestGlyphSlotRec existingTail{};
    GuestGlyphSlotRec insertedHead{};
    listFixture.clazz.init_slot = InsertHeadOnInit;
    gInsertedHead = &insertedHead;
    gInsertedTail = &existingTail;
    errno = 71;
    Require(FT_New_GlyphSlot_nid_postfix(&listFixture.face, reinterpret_cast<GuestGlyphSlotRec**>(0x10)) == 0x06 && errno == 71,
        "New_GlyphSlot rollback on unwritable aslot returns Invalid_Argument without changing errno");
    Require(listFixture.face.glyph == &insertedHead && insertedHead.next == &existingTail && listFixture.recorder.allocs == listFixture.recorder.frees,
        "New_GlyphSlot rollback preserves newer face->glyph head added during init_slot");
    gInsertedHead = nullptr;
    gInsertedTail = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(const_cast<GuestFaceRec*>(&readOnlyFace), &slot) == 0x06 && errno == 71 && !readOnlyFace.glyph && roRecorder.allocs == roRecorder.frees,
        "New_GlyphSlot refuses a read-only face without leaking or changing errno");

    fixture.face.driver = reinterpret_cast<void*>(0x10);
    Require(FT_New_GlyphSlot_nid_postfix(&fixture.face, &slot) == 0x06 && errno == 71,
        "New_GlyphSlot refuses an unreadable driver pointer without changing errno");

    DriverFixture badRootClazz;
    badRootClazz.driver.root_clazz = reinterpret_cast<GuestModuleClass*>(0x10);
    slot = reinterpret_cast<GuestGlyphSlotRec*>(0x1234);
    errno = 71;
    Require(FT_New_GlyphSlot_nid_postfix(&badRootClazz.face, &slot) == 0x06 && errno == 71,
        "New_GlyphSlot stops on an unreadable root_clazz without changing errno");
    Require(calls.inits == 0 && badRootClazz.recorder.allocs == badRootClazz.recorder.frees && !badRootClazz.face.glyph,
        "New_GlyphSlot does not call init_slot or leak on an unreadable root_clazz");

    DriverFixture corruptFace;
    corruptFace.clazz.init_slot = CorruptFaceOnInit;
    gCorruptTargetSlot = nullptr;
    gSavedInternal = nullptr;
    gSavedLoader = nullptr;
    errno = 71;
    Require(FT_New_GlyphSlot_nid_postfix(&corruptFace.face, &slot) == 0x24 && errno == 71,
        "Rollback cleans up slot allocations even when init_slot corrupts slot->face without changing errno");
    Require(calls.dones == 1 && corruptFace.recorder.allocs == corruptFace.recorder.frees && !corruptFace.face.glyph,
        "Rollback calls done_slot and frees all slot, internal and loader allocations without leaking");

    ft_glyphslot_free_bitmap_nid_postfix(reinterpret_cast<GuestGlyphSlotRec*>(0x10));
    Require(errno == 71, "free_bitmap ignores an unreadable slot without changing errno");
    ft_glyphslot_set_bitmap_nid_postfix(reinterpret_cast<GuestGlyphSlotRec*>(0x10), buffer);
    Require(errno == 71, "set_bitmap ignores an unreadable slot without changing errno");
    ft_glyphslot_set_bitmap_nid_postfix(const_cast<GuestGlyphSlotRec*>(&readOnlySlot), buffer);
    Require(errno == 71 && !readOnlySlot.bitmap.buffer, "set_bitmap ignores a read-only slot without changing errno");
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(reinterpret_cast<GuestGlyphSlotRec*>(0x10), 8) == 0x06 && errno == 71,
        "alloc_bitmap refuses an unreadable slot without changing errno");
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(const_cast<GuestGlyphSlotRec*>(&readOnlySlot), 8) == 0x06 && errno == 71 && !readOnlySlot.bitmap.buffer && roRecorder.allocs == roRecorder.frees,
        "alloc_bitmap refuses a read-only slot without leaking or changing errno");

    static unsigned char ownedDummy[4]{};
    static GuestSlotInternalRec roOwnedInternal{.flags = FreeType::GlyphOwnBitmap};
    static const GuestGlyphSlotRec readOnlyOwnedSlot{
        .face = const_cast<GuestFaceRec*>(&readOnlyFace),
        .bitmap = {.buffer = ownedDummy},
        .internal = &roOwnedInternal};
    roRecorder.allocs = 0;
    roRecorder.frees = 0;
    errno = 71;
    ft_glyphslot_set_bitmap_nid_postfix(const_cast<GuestGlyphSlotRec*>(&readOnlyOwnedSlot), buffer);
    Require(errno == 71 && readOnlyOwnedSlot.bitmap.buffer == ownedDummy && roRecorder.frees == 0 && (roOwnedInternal.flags & FreeType::GlyphOwnBitmap) != 0,
        "set_bitmap keeps owned bitmap intact when slot is read-only");
    Require(ft_glyphslot_alloc_bitmap_nid_postfix(const_cast<GuestGlyphSlotRec*>(&readOnlyOwnedSlot), 8) == 0x06 && errno == 71 && readOnlyOwnedSlot.bitmap.buffer == ownedDummy && roRecorder.allocs == 0 && roRecorder.frees == 0,
        "alloc_bitmap keeps owned bitmap intact when slot is read-only");
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
    NewSlot();
    NewSlotWithoutOutlines();
    NewSlotArguments();
    NewSlotFailures();
    GuestPointers();
    std::puts("Guest FreeType Ot glyph slot tests passed");
    return 0;
}
