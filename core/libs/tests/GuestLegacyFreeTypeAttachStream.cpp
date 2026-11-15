#include "Type1Fixture.hpp"
#include "prx/libfreetype/include/AttachStream.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType attach stream check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

int closes = 0;

std::uint64_t APS5_VABI ReadBytes(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* buffer, std::uint64_t count) {
    if (offset >= stream->size) return 0;
    const auto available = std::min(count, stream->size - offset);
    std::memcpy(buffer, stream->base + offset, static_cast<std::size_t>(available));
    return available;
}

void APS5_VABI CloseBytes(GuestStreamRec*) {
    ++closes;
}

struct FacePair {
    std::vector<unsigned char> font;
    GuestLibrary library{};
    FT_Library native = nullptr;
    GuestFace guest;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> bytes) : font(std::move(bytes)) {
        Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, font.data(), static_cast<FT_Long>(font.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0 && FT_Done_Face(oracle) == 0);
        Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
    }

    void KerningMatches(std::uint32_t left, std::uint32_t right, FT_Pos expected) const {
        FT_Vector actual{7, 7};
        FT_Vector reference{7, 7};
        Require(FT_Get_Kerning(guest.host, left, right, FT_KERNING_UNSCALED, &actual) == FT_Get_Kerning(oracle, left, right, FT_KERNING_UNSCALED, &reference));
        Require(actual.x == reference.x && actual.y == reference.y && reference.x == expected);
    }
};

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-attach-stream-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "app0"));
    std::vector<unsigned char> afm;
    {
        const std::string text = Type1Fixture::Afm();
        afm.assign(text.begin(), text.end());
        std::ofstream file(root / "app0" / "fixture.afm", std::ios::binary);
        file.write(reinterpret_cast<const char*>(afm.data()), static_cast<std::streamsize>(afm.size()));
        Require(static_cast<bool>(file));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    const std::string nativePath = (root / "app0" / "fixture.afm").string();
    {
        FacePair pair(Type1Fixture::Font());
        const std::uint32_t glyph = FT_Get_Char_Index(pair.oracle, 'A');
        Require(glyph != 0 && glyph == FT_Get_Char_Index(pair.guest.host, 'A'));
        pair.KerningMatches(glyph, glyph, 0);

        FT_Open_Args memory{};
        memory.flags = FT_OPEN_MEMORY;
        memory.memory_base = afm.data();
        memory.memory_size = static_cast<FT_Long>(afm.size());
        GuestOpenArgs guestMemory{};
        guestMemory.flags = FT_OPEN_MEMORY;
        guestMemory.memory_base = afm.data();
        guestMemory.memory_size = static_cast<std::int64_t>(afm.size());
        Require(FT_Attach_Stream(pair.oracle, &memory) == 0);
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &guestMemory) == 0);
        pair.KerningMatches(glyph, glyph, -50);

        GuestStreamRec stream{};
        stream.base = afm.data();
        stream.size = afm.size();
        stream.read = ReadBytes;
        stream.close = CloseBytes;
        GuestOpenArgs guestStream{};
        guestStream.flags = FT_OPEN_STREAM;
        guestStream.stream = &stream;
        closes = 0;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &guestStream) == 0);
        Require(closes == 1);
        pair.KerningMatches(glyph, glyph, -50);

        GuestStreamRec unreadable{};
        unreadable.base = afm.data();
        unreadable.size = afm.size();
        GuestOpenArgs baseOnly{};
        baseOnly.flags = FT_OPEN_STREAM;
        baseOnly.stream = &unreadable;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &baseOnly) == 0);
        pair.KerningMatches(glyph, glyph, -50);

        GuestOpenArgs guestPath{};
        guestPath.flags = FT_OPEN_PATHNAME;
        guestPath.pathname = const_cast<char*>("/app0/fixture.afm");
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &guestPath) == FT_Attach_File(pair.oracle, nativePath.c_str()));
        pair.KerningMatches(glyph, glyph, -50);

        GuestOpenArgs missing{};
        missing.flags = FT_OPEN_PATHNAME;
        missing.pathname = const_cast<char*>("/app0/missing.afm");
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &missing) == FT_Err_Cannot_Open_Resource);

        GuestOpenArgs nullPath{};
        nullPath.flags = FT_OPEN_PATHNAME;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &nullPath) == FT_Err_Invalid_Argument);

        Require(FT_Attach_Stream_nid_postfix(nullptr, &guestMemory) == FT_Err_Invalid_Face_Handle);
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, nullptr) == FT_Err_Invalid_Argument);

        GuestOpenArgs noSource{};
        noSource.flags = 0;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &noSource) == FT_Err_Invalid_Argument);

        GuestOpenArgs nullStream{};
        nullStream.flags = FT_OPEN_STREAM;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &nullStream) == FT_Err_Invalid_Argument);

        GuestOpenArgs mixed{};
        mixed.flags = FT_OPEN_STREAM | FT_OPEN_MEMORY;
        mixed.memory_base = afm.data();
        mixed.memory_size = static_cast<std::int64_t>(afm.size());
        mixed.stream = &stream;
        closes = 0;
        Require(FT_Attach_Stream_nid_postfix(&pair.guest.rec, &mixed) == FT_Err_Invalid_Argument);
        Require(closes == 1);
        pair.KerningMatches(glyph, glyph, -50);
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return 0;
}
