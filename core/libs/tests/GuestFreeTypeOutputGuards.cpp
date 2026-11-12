#include "prx/libSceFreeType/include/Api.hpp"
#include "FontFixture.hpp"
#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/include/PostScriptInfo.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/libkernel.hpp"

#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace FreeType;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "FreeType output guard check failed at line %u\n", location.line());
        std::abort();
    }
}

static std::vector<unsigned char> NamedFont() {
    std::vector<unsigned char> post(32, 0);
    post[1] = 2;
    for (const int value : {3, 0, 258, 259}) FontFixture::Put16(post, value);
    for (const std::string name : {"alpha", "beta"}) {
        post.push_back(static_cast<unsigned char>(name.size()));
        post.insert(post.end(), name.begin(), name.end());
    }
    auto names = FontFixture::Words({0, 1, 18, 3, 1, 0x0409, 6, 8, 0});
    for (const unsigned char ch : std::string("Name")) FontFixture::Put16(names, ch);
    return FontFixture::BuildFont(3, {{"post", post}, {"name", names}});
}

static std::vector<unsigned char> Type42() {
    const auto sfnt = SquareGlyphFont('A', 700);
    std::string font = "%!PS-TrueTypeFont-1.0: AnyPS5Guard 1.0\n11 dict begin\n"
        "/FontName /AnyPS5Guard def\n/FontType 42 def\n/PaintType 0 def\n"
        "/FontMatrix [1 0 0 1 0 0] def\n/FontBBox [0 0 700 800] def\n"
        "/Encoding StandardEncoding def\n/FontInfo 9 dict dup begin\n"
        "/version (1.5) def\n/FullName (AnyPS5 Guard) def\n/ItalicAngle 0 def\n/isFixedPitch false def\n"
        "/UnderlinePosition -100 def\n/UnderlineThickness 50 def\n"
        "end readonly def\n/CharStrings 2 dict dup begin\n/.notdef 0 def\n/A 1 def\nend readonly def\n/sfnts [<";
    constexpr char digits[] = "0123456789ABCDEF";
    for (std::size_t index = 0; index < sfnt.size(); ++index) {
        if (index != 0 && index % 64 == 0) font += ">\n<";
        font += digits[sfnt[index] >> 4];
        font += digits[sfnt[index] & 15];
    }
    font += ">] def\nFontName currentdict end definefont pop\n";
    return {font.begin(), font.end()};
}

struct Face {
    std::vector<unsigned char> bytes;
    GuestFace guest;

    Face(GuestLibrary& library, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        SyncFace(guest);
    }

    ~Face() { Require(FT_Done_Face(guest.host) == 0); }

    GuestFaceRec* Rec() { return &guest.rec; }
};

class GuestOutput {
public:
    static constexpr std::size_t Page = 0x4000;

    GuestOutput() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page * 2, 0, 0, &physical) == 0);
        Require(sceKernelMapDirectMemory(&mapping, Page * 2, 3, 0, physical, 0) == 0);
        Require(GuestArena::GuestArenaContains_nid_postfix(mapping, Page * 2));
        std::fill_n(static_cast<unsigned char*>(mapping), Page * 2, 0xa5);
    }

    ~GuestOutput() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), Page * 2) == 0);
        Require(sceKernelReleaseDirectMemory(physical, Page * 2) == 0);
    }

    template <typename TValue>
    TValue* At(std::size_t page) const {
        return reinterpret_cast<TValue*>(static_cast<unsigned char*>(mapping) + Page * page + 16);
    }

    void Protect(std::size_t page, int protection) const {
        Require(sceKernelMprotect(static_cast<unsigned char*>(mapping) + Page * page, Page, protection) == 0);
#ifdef _WIN32
        if (protection == 1) {
            MEMORY_BASIC_INFORMATION information{};
            Require(VirtualQuery(At<unsigned char>(page), &information, sizeof(information)) == sizeof(information));
            Require(information.Protect == PAGE_READONLY);
        }
#endif
    }

    template <typename TCall>
    void Reject(TCall call, int error = FT_Err_Invalid_Argument, std::source_location location = std::source_location::current()) const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        const std::vector<unsigned char> before(bytes, bytes + Page * 2);
        Require(call() == error, location);
        Require(std::memcmp(mapping, before.data(), before.size()) == 0, location);
    }

private:
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

static void CheckInit() {
    Require(FT_Init_FreeType_nid_postfix(nullptr) == FT_Err_Invalid_Argument);
    GuestOutput output;
    auto* library = output.At<GuestLibrary*>(0);
    output.Protect(0, 1);
    output.Reject([&] { return FT_Init_FreeType_nid_postfix(library); });
    output.Protect(0, 3);
    Require(FT_Init_FreeType_nid_postfix(library) == 0 && *library != nullptr);
    Require(FT_Done_FreeType_nid_postfix(*library) == 0);
}

static void CheckNames(GuestFaceRec* face) {
    GuestOutput output;
    auto* name = output.At<GuestSfntName>(0);
    auto* glyph = output.At<char>(1);
#ifdef _WIN32
    FT_SfntName nativeName{};
    char nativeGlyph[16]{};
    output.Protect(0, 1);
    output.Protect(1, 1);
    output.Reject([&] { return FT_Get_Sfnt_Name_nid_postfix(face, 0, name); });
    output.Reject([&] { return FT_Get_Sfnt_Name_nid_postfix(face, 7, name); });
    output.Reject([&] { return FT_Get_Sfnt_Name_nid_postfix(nullptr, 0, name); }, FT_Get_Sfnt_Name(nullptr, 0, &nativeName));
    output.Reject([&] { return FT_Get_Glyph_Name_nid_postfix(face, 1, glyph, 16); });
    output.Reject([&] { return FT_Get_Glyph_Name_nid_postfix(face, 9, glyph, 16); });
    output.Reject([&] { return FT_Get_Glyph_Name_nid_postfix(nullptr, 1, glyph, 16); }, FT_Get_Glyph_Name(nullptr, 1, nativeGlyph, 16));
    output.Protect(0, 3);
    output.Protect(1, 3);
#endif
    Require(FT_Get_Sfnt_Name_nid_postfix(face, 0, name) == 0 && name->name_id == 6 && name->string_len == 8);
    Require(FT_Get_Glyph_Name_nid_postfix(face, 1, glyph, 16) == 0 && std::strcmp(glyph, "alpha") == 0);
    Require(FT_Get_Glyph_Name_nid_postfix(face, 9, glyph, 16) == FT_Err_Invalid_Glyph_Index && glyph[0] == 0);
}

static void CheckTables(GuestFaceRec* face) {
    GuestOutput output;
    auto* tag = output.At<std::uint64_t>(0);
    auto* length = output.At<std::uint64_t>(1);
    auto* buffer = output.At<unsigned char>(0) + 32;
#ifdef _WIN32
    FT_ULong nativeTag = 0;
    for (const auto [tagPage, lengthPage] : {std::pair{1, 3}, std::pair{3, 1}, std::pair{1, 1}}) {
        output.Protect(0, tagPage);
        output.Protect(1, lengthPage);
        output.Reject([&] { return FT_Sfnt_Table_Info_nid_postfix(face, 0, tag, length); });
        output.Protect(0, 3);
        output.Protect(1, 3);
    }
    output.Protect(0, 1);
    output.Reject([&] { return FT_Sfnt_Table_Info_nid_postfix(face, 0, tag, nullptr); });
    output.Reject([&] { return FT_Sfnt_Table_Info_nid_postfix(nullptr, 0, tag, nullptr); }, FT_Sfnt_Table_Info(nullptr, 0, &nativeTag, nullptr));
    *length = 4;
    output.Reject([&] { return FT_Load_Sfnt_Table_nid_postfix(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 0, buffer, length); });
    output.Protect(0, 3);
    output.Protect(1, 1);
    output.Reject([&] { return FT_Sfnt_Table_Info_nid_postfix(face, 0, nullptr, length); });
    output.Protect(1, 3);
    *length = 0;
    output.Protect(1, 1);
    output.Reject([&] { return FT_Load_Sfnt_Table_nid_postfix(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 0, nullptr, length); });
    output.Protect(1, 3);
#endif
    Require(FT_Sfnt_Table_Info_nid_postfix(face, 0, tag, length) == 0 && *tag != 0 && *length != 0);
    *length = 0;
    Require(FT_Load_Sfnt_Table_nid_postfix(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 0, nullptr, length) == 0 && *length >= 4);
    *length = 4;
    Require(FT_Load_Sfnt_Table_nid_postfix(face, FT_MAKE_TAG('h', 'e', 'a', 'd'), 0, buffer, length) == 0);
    Require(buffer[0] == 0 && buffer[1] == 1 && buffer[2] == 0 && buffer[3] == 0);
}

static void CheckPostScript(GuestFaceRec* face) {
    GuestOutput output;
    auto* info = output.At<GuestPSFontInfo>(0);
#ifdef _WIN32
    PS_FontInfoRec nativeInfo{};
    output.Protect(0, 1);
    output.Reject([&] { return FT_Get_PS_Font_Info_nid_postfix(face, info); });
    output.Reject([&] { return FT_Get_PS_Font_Info_nid_postfix(nullptr, info); }, FT_Get_PS_Font_Info(nullptr, &nativeInfo));
    output.Protect(0, 3);
#endif
    Require(FT_Get_PS_Font_Info_nid_postfix(face, info) == 0);
    Require(info->fullName != nullptr && std::strcmp(info->fullName, "AnyPS5 Guard") == 0);
    Require(info->underlinePosition == -100 && info->underlineThickness == 50);
}

int main() {
    CheckInit();
    GuestLibrary library{};
    Require(FT_Init_FreeType(&library.host) == 0);
    {
        Face named(library, NamedFont());
        CheckNames(named.Rec());
        CheckTables(named.Rec());
        Face type42(library, Type42());
        CheckPostScript(type42.Rec());
    }
    Require(FT_Done_FreeType(library.host) == 0);
}
