#include "FontFixture.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType SFNT check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFaceRec* guest = nullptr;
    FT_Face native = nullptr;

    FacePair(GuestLibrary* library, FT_Library nativeLibrary, std::vector<unsigned char> data) : bytes(std::move(data)) {
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
    }

    ~FacePair() {
        Require(FT_Done_Face_nid_postfix(guest) == 0 && FT_Done_Face(native) == 0);
    }
};

static std::uint32_t Read32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) value = (value << 8) | bytes.at(offset + index);
    return value;
}

static void InfoMatches(GuestFaceRec* guest, FT_Face native, std::uint32_t index, bool withTag, bool withLength, int& comparisons) {
    constexpr std::uint64_t sentinel = 0xFEDCBA9876543210ull;
    std::array<std::uint64_t, 4> output{sentinel, sentinel, sentinel, sentinel};
    FT_ULong tag = 0;
    FT_ULong length = 0;
    const auto error = FT_Sfnt_Table_Info(native, index, withTag ? &tag : nullptr, withLength ? &length : nullptr);
    Require(FT_Sfnt_Table_Info_nid_postfix(guest, index, withTag ? &output[1] : nullptr, withLength ? &output[2] : nullptr) == error);
    Require(output[0] == sentinel && output[3] == sentinel);
    Require(output[1] == (error == 0 && withTag ? static_cast<std::uint64_t>(tag) : sentinel));
    Require(output[2] == (error == 0 && withLength ? static_cast<std::uint64_t>(length) : sentinel));
    if (withTag && withLength) {
        FT_ULong alias = 0;
        std::uint64_t guestAlias = sentinel;
        Require(FT_Sfnt_Table_Info(native, index, &alias, &alias) == error);
        Require(FT_Sfnt_Table_Info_nid_postfix(guest, index, &guestAlias, &guestAlias) == error);
        Require(guestAlias == (error == 0 ? static_cast<std::uint64_t>(alias) : sentinel));
    }
    ++comparisons;
}

static FT_Error ReadMatches(GuestFaceRec* guest, FT_Face native, std::uint32_t tag, std::int64_t offset, std::uint64_t requested, bool withLength, int& comparisons) {
    std::array<unsigned char, 1024> actual{};
    actual.fill(0xA5);
    auto expected = actual;
    std::uint64_t actualLength = requested;
    FT_ULong nativeLength = static_cast<FT_ULong>(std::min(requested, static_cast<std::uint64_t>(std::numeric_limits<FT_ULong>::max())));
    const auto nativeOffset = static_cast<FT_Long>(withLength && requested == 0 ? 0 : offset);
    const auto error = FT_Load_Sfnt_Table(native, tag, nativeOffset, expected.data() + 8, withLength ? &nativeLength : nullptr);
    Require(FT_Load_Sfnt_Table_nid_postfix(guest, tag, offset, actual.data() + 8, withLength ? &actualLength : nullptr) == error);
    Require(actual == expected);
    Require(actualLength == (withLength && requested == 0 && error == 0 ? static_cast<std::uint64_t>(nativeLength) : requested));
    if (guest) Require(FaceOf(guest)->host->stream->pos == native->stream->pos);
    ++comparisons;
    return error;
}

static FT_ULong VirtualRead(FT_Stream stream, FT_ULong offset, unsigned char* buffer, FT_ULong count) {
    if (offset >= stream->size || !buffer) return 0;
    const auto bytes = std::min(count, stream->size - offset);
    for (FT_ULong index = 0; index < bytes; ++index) buffer[index] = static_cast<unsigned char>((offset + index) ^ 0x5A);
    return bytes;
}

static void WideStreamAccess(FacePair& face, int& comparisons) {
    if constexpr (sizeof(FT_Long) < sizeof(std::int64_t)) {
        FT_Stream stream = FaceOf(face.guest)->host->stream;
        FT_Stream native = face.native->stream;
        const auto original = stream->size;
        const auto originalNative = native->size;
        const auto originalRead = stream->read;
        const auto originalNativeRead = native->read;
        stream->size = native->size = static_cast<FT_ULong>(std::numeric_limits<FT_Long>::max()) + 64;
        Require(ReadMatches(face.guest, face.native, 0, std::numeric_limits<std::int64_t>::max(), 0, true, comparisons) == 0);
        Require(ReadMatches(face.guest, face.native, 0, 0, 1, true, comparisons) == 0);
        Require(ReadMatches(face.guest, face.native, 0x54455354u, 0, 1, true, comparisons) == 0);
        stream->read = native->read = VirtualRead;
        const auto offset = static_cast<std::int64_t>(std::numeric_limits<FT_Long>::max()) + 6;
        Require(ReadMatches(face.guest, face.native, 0, offset, 3, true, comparisons) == 0);
        std::array<unsigned char, 3> pixels{};
        std::uint64_t length = pixels.size();
        Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, 0, offset, pixels.data(), &length) == 0 && length == pixels.size());
        for (std::size_t index = 0; index < pixels.size(); ++index) Require(pixels[index] == static_cast<unsigned char>((offset + static_cast<std::int64_t>(index)) ^ 0x5A));
        stream->size = original;
        native->size = originalNative;
        stream->read = originalRead;
        native->read = originalNativeRead;
    }
}

struct WatchedMetadataPages {
    std::uintptr_t tag;
    std::uintptr_t length;
    bool tagWritten = false;
    bool lengthWritten = false;

    static void Mark(void* context, std::uintptr_t begin, std::uintptr_t end) {
        auto& pages = *static_cast<WatchedMetadataPages*>(context);
        if (begin <= pages.tag && pages.tag < end) pages.tagWritten = true;
        if (begin <= pages.length && pages.length < end) pages.lengthWritten = true;
    }
};

static void WatchedMetadata(FacePair& face) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix()) {
        std::puts("FreeType SFNT watched metadata: guest arena unavailable, skipped");
        return;
    }
#ifdef _WIN32
    if (!GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::puts("FreeType SFNT watched metadata: write watch unavailable, skipped");
        return;
    }
#else
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::puts("FreeType SFNT watched metadata: write watch unavailable, skipped");
        return;
    }
#endif
    constexpr std::size_t span = 0x10000;
    auto* block = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(span, span));
    GuestArena::GuestArenaCommit_nid_postfix(block, span, 0x04, span);
    auto* tag = reinterpret_cast<std::uint64_t*>(block + 0x100);
    auto* length = reinterpret_cast<std::uint64_t*>(block + 0x4100);
    constexpr std::uint64_t sentinel = 0xFEDCBA9876543210ull;
    *tag = sentinel;
    *length = sentinel;
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, span);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(block), span));
#endif
    const auto collect = [&]() {
        WatchedMetadataPages dirty{reinterpret_cast<std::uintptr_t>(tag), reinterpret_cast<std::uintptr_t>(length)};
#ifdef _WIN32
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(block), span, pages.data(), &count, true));
        for (std::size_t index = 0; index < count; ++index) {
            const auto begin = reinterpret_cast<std::uintptr_t>(pages[index]);
            WatchedMetadataPages::Mark(&dirty, begin, begin + 0x1000);
        }
#else
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(block), span, WatchedMetadataPages::Mark, &dirty));
#endif
        return dirty;
    };
    const auto expect = [&](bool tagWritten, bool lengthWritten) {
        const auto dirty = collect();
        Require(dirty.tagWritten == tagWritten && dirty.lengthWritten == lengthWritten);
    };
    collect();
    expect(false, false);
    FT_ULong nativeTag = 0;
    FT_ULong nativeLength = 0;
    Require(FT_Sfnt_Table_Info(face.native, 0, &nativeTag, &nativeLength) == 0);
    Require(FT_Sfnt_Table_Info_nid_postfix(face.guest, 0, tag, length) == 0);
    Require(*tag == nativeTag && *length == nativeLength);
    expect(true, true);
    FT_ULong nativeAlias = 0;
    Require(FT_Sfnt_Table_Info(face.native, 0, &nativeAlias, &nativeAlias) == 0);
    {
        GuestArena::HostWrite write(tag, sizeof(*tag));
        *tag = sentinel;
    }
    collect();
    expect(false, false);
    Require(FT_Sfnt_Table_Info_nid_postfix(face.guest, 0, tag, tag) == 0 && *tag == nativeAlias);
    expect(true, false);
    {
        GuestArena::HostWrite write(length, sizeof(*length));
        *length = 0;
    }
    collect();
    expect(false, false);
    FT_ULong queriedLength = 0;
    Require(FT_Load_Sfnt_Table(face.native, nativeTag, 0, nullptr, &queriedLength) == 0);
    Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, nativeTag, 0, reinterpret_cast<unsigned char*>(length), length) == 0);
    Require(*length == queriedLength);
    expect(false, true);
    const auto retainedTag = *tag;
    const auto retainedLength = *length;
    Require(FT_Sfnt_Table_Info_nid_postfix(nullptr, 0, tag, length) == FT_Err_Invalid_Face_Handle);
    Require(*tag == retainedTag && *length == retainedLength);
    expect(false, false);
#ifndef _WIN32
    Require(GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, span));
#endif
    GuestArena::GuestArenaReset_nid_postfix(block, span);
    GuestArena::GuestArenaRelease_nid_postfix(block, span);
    std::puts("FreeType SFNT watched metadata: split outputs, alias and zero-length query recorded");
}

int main() {
    GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    int information = 0;
    int reads = 0;
    {
        const auto font = FontFixture::BuildFont(1, {{"TEST", {11, 22, 33, 44, 55, 66, 77}},
            {"TAIL", {7, 6, 5, 4, 3, 2, 1, 0, 255, 254, 253, 252, 251, 250, 249}}, {"ZERO", {}},
            {std::string("\xF1\x23\xAB\xCD", 4), {19, 29, 39, 49, 59, 69, 79, 89}}});
        FacePair face(library, nativeLibrary, font);
        WatchedMetadata(face);
        const unsigned tableCount = (static_cast<unsigned>(font[4]) << 8) | font[5];
        Require(font.size() + 16 < 1024);
        std::uint64_t count = 0;
        Require(FT_Sfnt_Table_Info_nid_postfix(face.guest, std::numeric_limits<std::uint32_t>::max(), nullptr, &count) == 0 && count == tableCount);
        for (const auto index : {0u, tableCount - 1, tableCount, std::numeric_limits<std::uint32_t>::max()}) {
            for (const bool withTag : {false, true}) {
                for (const bool withLength : {false, true}) {
                    InfoMatches(face.guest, face.native, index, withTag, withLength, information);
                    InfoMatches(nullptr, nullptr, index, withTag, withLength, information);
                }
            }
        }
        for (unsigned index = 0; index < tableCount; ++index) {
            const std::size_t directory = 12 + index * 16;
            const auto tag = Read32(font, directory);
            const auto size = Read32(font, directory + 12);
            std::uint64_t actualTag = 0;
            std::uint64_t actualLength = 0;
            Require(FT_Sfnt_Table_Info_nid_postfix(face.guest, index, &actualTag, &actualLength) == 0 && actualTag == tag && actualLength == size);
            InfoMatches(face.guest, face.native, index, true, true, information);
            for (const auto offset : {0ll, 1ll, -1ll, static_cast<long long>(size), static_cast<long long>(size + 1), 2147483647ll, -2147483648ll}) {
                ReadMatches(face.guest, face.native, tag, offset, 0, true, reads);
                ReadMatches(face.guest, face.native, tag, offset, size, false, reads);
                for (const auto requested : {1ull, 3ull, static_cast<unsigned long long>(size), static_cast<unsigned long long>(size + 3)}) {
                    ReadMatches(face.guest, face.native, tag, offset, requested, true, reads);
                }
            }
            ReadMatches(face.guest, face.native, tag, std::numeric_limits<std::int64_t>::max(), 0, true, reads);
            ReadMatches(face.guest, face.native, tag, std::numeric_limits<std::int64_t>::min(), 0, true, reads);
        }
        for (const auto offset : {0ll, 1ll, -1ll, static_cast<long long>(font.size()), static_cast<long long>(font.size() - 3)}) {
            for (const auto requested : {0ull, 1ull, 16ull, static_cast<unsigned long long>(font.size()), std::numeric_limits<unsigned long long>::max()}) {
                ReadMatches(face.guest, face.native, 0, offset, requested, true, reads);
            }
            ReadMatches(face.guest, face.native, 0, offset, 0, false, reads);
        }
        Require(ReadMatches(face.guest, face.native, 0, static_cast<std::int64_t>(font.size() - 3), 8, true, reads) != 0);
        std::array<std::uint64_t, 8> alias{};
        alias.fill(0xA5A5A5A5A5A5A5A5ull);
        alias[0] = 16;
        auto nativeAlias = alias;
        FT_ULong nativeAliasLength = 16;
        Require(FT_Load_Sfnt_Table(face.native, 0, 0, reinterpret_cast<unsigned char*>(nativeAlias.data()), &nativeAliasLength) == 0);
        Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, 0, 0, reinterpret_cast<unsigned char*>(alias.data()), &alias[0]) == 0);
        Require(alias == nativeAlias && std::memcmp(alias.data(), font.data(), 16) == 0 && alias[2] == 0xA5A5A5A5A5A5A5A5ull);
        const auto missing = ReadMatches(face.guest, face.native, 0x4D495353u, 0, 0, true, reads);
        Require(missing == FT_Err_Table_Missing);
        for (const auto tag : {0x100000000ull, 0x168656164ull, std::numeric_limits<unsigned long long>::max()}) {
            std::uint64_t length = 7;
            unsigned char byte = 0xA5;
            Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, tag, 0, &byte, &length) == missing && length == 7 && byte == 0xA5);
            Require(FT_Load_Sfnt_Table_nid_postfix(nullptr, tag, 0, &byte, &length) == FT_Err_Invalid_Face_Handle);
        }
        for (const auto offset : std::array<std::int64_t, 4>{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(), -2147483649ll, 2147483648ll}) {
            std::uint64_t length = 1;
            unsigned char byte = 0xA5;
            Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, 0, offset, &byte, &length) == FT_Err_Invalid_Stream_Operation && byte == 0xA5 && length == 1);
            Require(FT_Load_Sfnt_Table_nid_postfix(face.guest, 0x4D495353u, offset, nullptr, nullptr) == missing);
        }
        const std::string bdf = "STARTFONT 2.1\nFONT -anyps5-sfnt-medium-r-normal--1-10-75-75-c-10-iso10646-1\nSIZE 1 75 75\n"
            "FONTBOUNDINGBOX 1 1 0 0\nSTARTPROPERTIES 4\nFONT_ASCENT 1\nFONT_DESCENT 0\nCHARSET_REGISTRY \"ISO10646\"\nCHARSET_ENCODING \"1\"\n"
            "ENDPROPERTIES\nCHARS 1\nSTARTCHAR A\nENCODING 65\nSWIDTH 1000 0\nDWIDTH 1 0\nBBX 1 1 0 0\nBITMAP\n80\nENDCHAR\nENDFONT\n";
        FacePair bitmap(library, nativeLibrary, {bdf.begin(), bdf.end()});
        Require(!FT_IS_SFNT(bitmap.native));
        InfoMatches(bitmap.guest, bitmap.native, 0, true, true, information);
        InfoMatches(bitmap.guest, bitmap.native, 0, false, false, information);
        ReadMatches(bitmap.guest, bitmap.native, 0, 0, 0, true, reads);
        ReadMatches(bitmap.guest, bitmap.native, 0, 0, 0, false, reads);
        ReadMatches(nullptr, nullptr, 0, 0, 0, true, reads);
        WideStreamAccess(face, reads);
    }
    std::printf("FreeType SFNT: %d directory comparisons, %d read comparisons; wide inputs, aliases and short-read bytes checked\n", information, reads);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
