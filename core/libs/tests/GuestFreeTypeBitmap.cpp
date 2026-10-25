#include "prx/libSceFreeType/include/Api.hpp"
#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H
#include FT_MODULE_H

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType bitmap check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static GuestBitmap Guest(const FT_Bitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

static void Matches(const GuestBitmap& guest, const FT_Bitmap& native, bool comparePadding) {
    Require(guest.rows == native.rows && guest.width == native.width && guest.pitch == native.pitch);
    Require(guest.num_grays == native.num_grays && guest.pixel_mode == native.pixel_mode && guest.palette_mode == native.palette_mode);
    Require(guest.palette == native.palette && (guest.buffer == nullptr) == (native.buffer == nullptr));
    if (!guest.buffer) return;
    const auto pitch = static_cast<std::size_t>(native.pitch < 0 ? -native.pitch : native.pitch);
    const std::size_t width = comparePadding ? pitch : native.width;
    for (std::uint32_t row = 0; row < native.rows; ++row) {
        Require(std::memcmp(guest.buffer + row * pitch, native.buffer + row * pitch, width) == 0);
    }
}

static void TestFormat(GuestLibrary* library, FT_Library oracle, unsigned char mode, int bits) {
    constexpr std::uint32_t width = 10;
    constexpr std::uint32_t rows = 3;
    const int stride = static_cast<int>((width * bits + 7) / 8) + 2;
    std::vector<unsigned char> data(rows * static_cast<std::size_t>(stride));
    for (std::size_t index = 0; index < data.size(); ++index) data[index] = static_cast<unsigned char>(index * 37 + 11);
    if (mode == FT_PIXEL_MODE_BGRA) {
        for (std::uint32_t row = 0; row < rows; ++row) {
            for (std::uint32_t col = 0; col < width; ++col) {
                unsigned char* pixel = data.data() + row * stride + col * 4;
                pixel[0] = static_cast<unsigned char>(col * 7);
                pixel[1] = static_cast<unsigned char>(row * 23);
                pixel[2] = static_cast<unsigned char>(col * 11);
                pixel[3] = col == 0 ? 0 : static_cast<unsigned char>(128 + row * 63);
                if (col == 0) pixel[0] = pixel[1] = pixel[2] = 0;
            }
        }
    }
    const auto original = data;
    int palette = 17;
    for (const int sign : {-1, 1}) {
        const FT_Bitmap source{rows, width, sign * stride, data.data(), static_cast<unsigned short>(bits < 8 ? 1 << bits : 256), mode, 7, &palette};
        GuestBitmap guestSource = Guest(source);
        for (const int flow : {-1, 0, 1}) {
            FT_Bitmap native{};
            native.pitch = flow;
            GuestBitmap target{};
            target.pitch = flow;
            Require(FT_Bitmap_Copy_nid_postfix(library, &guestSource, &target) == FT_Bitmap_Copy(oracle, &source, &native));
            Require(target.buffer != source.buffer && native.buffer != source.buffer && target.buffer != native.buffer);
            Matches(target, native, true);
            const GuestBitmap before = target;
            Require(FT_Bitmap_Copy_nid_postfix(library, &target, &target) == 0);
            Require(target.buffer == before.buffer && target.pitch == before.pitch);
            Matches(target, native, true);
            data[0] ^= 0xFF;
            Require(target.buffer[flow != 0 && flow != sign ? (rows - 1) * stride : 0] == original[0]);
            data[0] ^= 0xFF;
            for (const int alignment : {0, 1, 2, 4, -4}) {
                Require(FT_Bitmap_Convert_nid_postfix(library, &guestSource, &target, alignment) == FT_Bitmap_Convert(oracle, &source, &native, alignment));
                Matches(target, native, false);
                Require(target.pixel_mode == FT_PIXEL_MODE_GRAY && target.rows == rows && target.width == width);
                Require(target.pitch == (flow < 0 || (flow == 0 && sign < 0) ? -1 : 1) * (alignment == 4 || alignment == -4 ? 12 : 10));
                Require(data == original);
            }
            Require(FT_Bitmap_Done_nid_postfix(library, &target) == FT_Bitmap_Done(oracle, &native));
            Matches(target, native, true);
            Require(target.rows == 0 && target.pitch == 0 && target.buffer == nullptr && target.palette == nullptr);
            Require(FT_Bitmap_Done_nid_postfix(library, &target) == 0);
        }
    }
}

struct Allocations {
    bool fail = false;
    std::unordered_set<void*> blocks;

    static void* Allocate(FT_Memory memory, long size) {
        auto& state = *static_cast<Allocations*>(memory->user);
        if (state.fail) return nullptr;
        void* block = std::malloc(static_cast<std::size_t>(size));
        if (block) Require(state.blocks.insert(block).second);
        return block;
    }

    static void Free(FT_Memory memory, void* block) {
        auto& state = *static_cast<Allocations*>(memory->user);
        Require(state.blocks.erase(block) == 1);
        std::free(block);
    }

    static void* Reallocate(FT_Memory, long, long, void*) {
        return nullptr;
    }
};

static void TestAllocationFailure() {
    Allocations guestAllocations;
    Allocations nativeAllocations;
    FT_MemoryRec_ guestMemory{&guestAllocations, Allocations::Allocate, Allocations::Free, Allocations::Reallocate};
    FT_MemoryRec_ nativeMemory{&nativeAllocations, Allocations::Allocate, Allocations::Free, Allocations::Reallocate};
    GuestLibrary library{};
    FT_Library oracle = nullptr;
    Require(FT_New_Library(&guestMemory, &library.host) == 0);
    Require(FT_New_Library(&nativeMemory, &oracle) == 0);
    const auto guestBase = guestAllocations.blocks.size();
    const auto nativeBase = nativeAllocations.blocks.size();
    unsigned char pixels[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const FT_Bitmap source{2, 5, 5, pixels, 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    const GuestBitmap guestSource = Guest(source);
    GuestBitmap target{};
    FT_Bitmap native{};
    for (const bool convert : {false, true}) {
        Require(FT_Bitmap_Copy_nid_postfix(&library, &guestSource, &target) == 0);
        Require(FT_Bitmap_Copy(oracle, &source, &native) == 0);
        Require(guestAllocations.blocks.size() == guestBase + 1 && nativeAllocations.blocks.size() == nativeBase + 1);
        guestAllocations.fail = nativeAllocations.fail = true;
        const FT_Error expected = convert ? FT_Bitmap_Convert(oracle, &source, &native, 4) : FT_Bitmap_Copy(oracle, &source, &native);
        const int error = convert ? FT_Bitmap_Convert_nid_postfix(&library, &guestSource, &target, 4) : FT_Bitmap_Copy_nid_postfix(&library, &guestSource, &target);
        Require(expected == FT_Err_Out_Of_Memory && error == expected);
        Matches(target, native, false);
        Require(target.buffer == nullptr && guestAllocations.blocks.size() == guestBase && nativeAllocations.blocks.size() == nativeBase);
        guestAllocations.fail = nativeAllocations.fail = false;
    }
    Require(FT_Bitmap_Convert_nid_postfix(&library, &guestSource, &target, 4) == 0);
    Require(FT_Bitmap_Convert(oracle, &source, &native, 4) == 0);
    Matches(target, native, false);
    Require(FT_Bitmap_Done_nid_postfix(&library, &target) == 0 && FT_Bitmap_Done(oracle, &native) == 0);
    Require(guestAllocations.blocks.size() == guestBase && nativeAllocations.blocks.size() == nativeBase);
    Require(FT_Done_Library(library.host) == 0 && FT_Done_Library(oracle) == 0);
    Require(guestAllocations.blocks.empty() && nativeAllocations.blocks.empty());
}

struct SlotFinalizer {
    GuestFace* face;
    GuestLibrary* library;
    bool called = false;

    static void Run(void* object) {
        auto* slot = static_cast<FT_GlyphSlot>(object);
        auto& state = *static_cast<SlotFinalizer*>(slot->generic.data);
        Require(slot->face->glyph == nullptr);
        Require(FT_Load_Glyph_nid_postfix(&state.face->rec, 0, FT_LOAD_DEFAULT) == FT_Err_Invalid_Face_Handle);
        Require(FT_Bitmap_Done_nid_postfix(state.library, &state.face->slot.bitmap) == 0);
        Require(slot->bitmap.buffer == nullptr && slot->bitmap.width == 0);
        state.called = true;
    }
};

static void TestGlyphSlot(GuestLibrary* library, FT_Library oracle) {
    const auto bytes = FontFixture::BuildFont(1, {});
    GuestFace face;
    face.library = library;
    Require(FT_New_Memory_Face(library->host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face.host) == 0);
    FT_Face nativeFace = nullptr;
    Require(FT_New_Memory_Face(oracle, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &nativeFace) == 0);
    SyncFace(face);
    unsigned char pixels[]{7, 31, 65, 127, 255, 17, 61, 101, 189, 239};
    const FT_Bitmap source{2, 5, 5, pixels, 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    const GuestBitmap guestSource = Guest(source);
    auto& target = face.slot.bitmap;
    auto& native = nativeFace->glyph->bitmap;
    Require(FT_Bitmap_Copy_nid_postfix(library, &guestSource, &target) == FT_Bitmap_Copy(oracle, &source, &native));
    Matches(target, native, true);
    Require(face.host->glyph->bitmap.buffer == target.buffer && face.host->glyph->bitmap.width == 5);
    GuestBitmap retained{};
    Require(FT_Bitmap_Copy_nid_postfix(library, &target, &retained) == 0);
    Require(retained.buffer != target.buffer && std::memcmp(retained.buffer, target.buffer, 10) == 0);
    Require(FT_Bitmap_Done_nid_postfix(library, &target) == FT_Bitmap_Done(oracle, &native));
    Require(face.host->glyph->bitmap.buffer == nullptr && face.host->glyph->bitmap.width == 0);
    Require(FT_Bitmap_Convert_nid_postfix(library, &guestSource, &target, 4) == FT_Bitmap_Convert(oracle, &source, &native, 4));
    Matches(target, native, false);
    Require(face.host->glyph->bitmap.buffer == target.buffer && face.host->glyph->bitmap.pitch == 8);
    Require(FT_Bitmap_Done_nid_postfix(library, &target) == 0 && FT_Bitmap_Done(oracle, &native) == 0);
    target.rows = 9;
    target.width = 17;
    face.host->glyph->bitmap.rows = 9;
    face.host->glyph->bitmap.width = 17;
    FT_Bitmap_New_nid_postfix(&target);
    Require(target.rows == 0 && face.host->glyph->bitmap.rows == 0 && face.host->glyph->bitmap.width == 0);
    Require(FT_Bitmap_Done_nid_postfix(library, &retained) == 0);
    Require(FT_Bitmap_Copy_nid_postfix(library, &guestSource, &target) == 0);
    SlotFinalizer finalizer{&face, library};
    face.host->glyph->generic = {&finalizer, SlotFinalizer::Run};
    Require(FT_Done_Face(face.host) == 0 && FT_Done_Face(nativeFace) == 0);
    Require(finalizer.called);
}

static void TestReusedStorage(GuestLibrary* library) {
    const auto bytes = FontFixture::BuildFont(1, {});
    alignas(GuestFace) std::byte storage[sizeof(GuestFace)]{};
    auto* face = new (storage) GuestFace;
    face->library = library;
    Require(FT_New_Memory_Face(library->host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face->host) == 0);
    FT_Face native = face->host;
    SyncFace(*face);
    void* address = &face->slot.bitmap;
    face->~GuestFace();
    auto* bitmap = new (address) GuestBitmap{};
    native->glyph->bitmap.width = 73;
    FT_Bitmap_New_nid_postfix(bitmap);
    Require(bitmap->width == 0 && native->glyph->bitmap.width == 73);
    Require(FT_Done_Face(native) == 0);
}

int main() {
    GuestLibrary* library = nullptr;
    FT_Library oracle = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && library != nullptr);
    Require(FT_Init_FreeType(&oracle) == 0);

    GuestBitmap empty{1, 2, 3, nullptr, 4, 5, 6, nullptr};
    FT_Bitmap_New_nid_postfix(&empty);
    FT_Bitmap nativeEmpty{};
    Matches(empty, nativeEmpty, true);
    Require(empty.rows == 0 && empty.width == 0 && empty.pitch == 0);
    FT_Bitmap_New_nid_postfix(nullptr);
    Require(FT_Bitmap_Copy_nid_postfix(nullptr, nullptr, nullptr) == FT_Err_Invalid_Library_Handle);
    Require(FT_Bitmap_Convert_nid_postfix(nullptr, nullptr, nullptr, 1) == FT_Err_Invalid_Library_Handle);
    Require(FT_Bitmap_Done_nid_postfix(nullptr, nullptr) == FT_Err_Invalid_Library_Handle);
    Require(FT_Bitmap_Copy_nid_postfix(library, nullptr, &empty) == FT_Err_Invalid_Argument);
    Require(FT_Bitmap_Copy_nid_postfix(library, &empty, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Bitmap_Convert_nid_postfix(library, nullptr, &empty, 1) == FT_Err_Invalid_Argument);
    Require(FT_Bitmap_Convert_nid_postfix(library, &empty, nullptr, 1) == FT_Err_Invalid_Argument);
    Require(FT_Bitmap_Done_nid_postfix(library, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Bitmap_Copy_nid_postfix(library, &empty, &empty) == 0);

    for (const auto [mode, bits] : {std::pair{FT_PIXEL_MODE_MONO, 1}, {FT_PIXEL_MODE_GRAY2, 2}, {FT_PIXEL_MODE_GRAY4, 4},
                                  {FT_PIXEL_MODE_GRAY, 8}, {FT_PIXEL_MODE_LCD, 8}, {FT_PIXEL_MODE_LCD_V, 8}, {FT_PIXEL_MODE_BGRA, 32}}) {
        TestFormat(library, oracle, static_cast<unsigned char>(mode), bits);
    }
    GuestBitmap target{};
    const GuestBitmap unsupported{0, 0, 0, nullptr, 0, FT_PIXEL_MODE_NONE, 0, nullptr};
    Require(FT_Bitmap_Copy_nid_postfix(library, &unsupported, &target) == 0);
    Require(target.buffer == nullptr);
    Require(FT_Bitmap_Convert_nid_postfix(library, &unsupported, &target, 1) == FT_Err_Invalid_Argument);
    Require(target.buffer == nullptr && target.width == 0);
    bool overlapThrew = false;
    try {
        FT_Bitmap_Convert_nid_postfix(library, &target, &target, 1);
    } catch (const std::runtime_error&) {
        overlapThrew = true;
    }
    Require(overlapThrew);
    unsigned char sharedPixel = 17;
    GuestBitmap shared{1, 1, 1, &sharedPixel, 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    GuestBitmap alias = shared;
    for (const bool convert : {false, true}) {
        overlapThrew = false;
        try {
            if (convert) FT_Bitmap_Convert_nid_postfix(library, &shared, &alias, 1);
            else FT_Bitmap_Copy_nid_postfix(library, &shared, &alias);
        } catch (const std::runtime_error&) {
            overlapThrew = true;
        }
        Require(overlapThrew && sharedPixel == 17 && alias.buffer == &sharedPixel);
    }
    Require(FT_Bitmap_Done_nid_postfix(library, &target) == 0);
    TestAllocationFailure();
    TestGlyphSlot(library, oracle);
    TestReusedStorage(library);
    Require(FT_Done_FreeType(library->host) == 0);
    delete library;
    Require(FT_Done_FreeType(oracle) == 0);
}
