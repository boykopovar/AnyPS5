#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType sfnt table check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
#define Same(field) Require(actual.field == expected.field)
#define SameArray(field) Require(std::memcmp(actual.field, expected.field, sizeof(actual.field)) == 0)

static void Put32(std::vector<unsigned char>& out, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out[offset + i] = static_cast<unsigned char>(value >> (24 - 8 * i));
}

static void Put16(std::vector<unsigned char>& out, std::size_t offset, std::uint16_t value) {
    out[offset] = static_cast<unsigned char>(value >> 8);
    out[offset + 1] = static_cast<unsigned char>(value);
}

static void Expect(const GuestTtHeader& actual, const TT_Header& expected) {
    Same(Table_Version);
    Same(Font_Revision);
    Same(CheckSum_Adjust);
    Same(Magic_Number);
    Same(Flags);
    Same(Units_Per_EM);
    for (int i = 0; i < 2; ++i) Require(actual.Created[i] == expected.Created[i] && actual.Modified[i] == expected.Modified[i]);
    Same(xMin);
    Same(yMin);
    Same(xMax);
    Same(yMax);
    Same(Mac_Style);
    Same(Lowest_Rec_PPEM);
    Same(Font_Direction);
    Same(Index_To_Loc_Format);
    Same(Glyph_Data_Format);
    Require(actual.Units_Per_EM == 1000 && actual.Magic_Number == 0x5F0F3CF5);
}

static void Expect(const GuestTtMaxProfile& actual, const TT_MaxProfile& expected) {
    Same(version);
    Same(numGlyphs);
    Same(maxPoints);
    Same(maxContours);
    Same(maxCompositePoints);
    Same(maxCompositeContours);
    Same(maxZones);
    Same(maxTwilightPoints);
    Same(maxStorage);
    Same(maxFunctionDefs);
    Same(maxInstructionDefs);
    Same(maxStackElements);
    Same(maxSizeOfInstructions);
    Same(maxComponentElements);
    Same(maxComponentDepth);
    Require(actual.version == 0x10000 && actual.numGlyphs == 3 && actual.maxZones == 2);
}

static void Expect(const GuestTtOs2& actual, const TT_OS2& expected) {
    Same(version);
    Same(xAvgCharWidth);
    Same(usWeightClass);
    Same(usWidthClass);
    Same(fsType);
    Same(ySubscriptXSize);
    Same(ySubscriptYSize);
    Same(ySubscriptXOffset);
    Same(ySubscriptYOffset);
    Same(ySuperscriptXSize);
    Same(ySuperscriptYSize);
    Same(ySuperscriptXOffset);
    Same(ySuperscriptYOffset);
    Same(yStrikeoutSize);
    Same(yStrikeoutPosition);
    Same(sFamilyClass);
    SameArray(panose);
    Same(ulUnicodeRange1);
    Same(ulUnicodeRange2);
    Same(ulUnicodeRange3);
    Same(ulUnicodeRange4);
    SameArray(achVendID);
    Same(fsSelection);
    Same(usFirstCharIndex);
    Same(usLastCharIndex);
    Same(sTypoAscender);
    Same(sTypoDescender);
    Same(sTypoLineGap);
    Same(usWinAscent);
    Same(usWinDescent);
    Same(ulCodePageRange1);
    Same(ulCodePageRange2);
    Same(sxHeight);
    Same(sCapHeight);
    Same(usDefaultChar);
    Same(usBreakChar);
    Same(usMaxContext);
    Same(usLowerOpticalPointSize);
    Same(usUpperOpticalPointSize);
    Require(actual.version == 5 && actual.fsType == 0x0104 && actual.xAvgCharWidth == -12);
    Require(actual.ulUnicodeRange1 == 0x80000001u && actual.ulUnicodeRange4 == 0xFFFFFFFFu);
    Require(actual.ulCodePageRange1 == 0x80000000u && actual.ulCodePageRange2 == 0x00000002u);
    Require(std::memcmp(actual.achVendID, "APS5", 4) == 0 && actual.sTypoAscender == 0x0300);
    Require(actual.usUpperOpticalPointSize == 0xFFFF);
}

static void Expect(const GuestTtHoriHeader& actual, const TT_HoriHeader& expected) {
    Same(Version);
    Same(Ascender);
    Same(Descender);
    Same(Line_Gap);
    Same(advance_Width_Max);
    Same(min_Left_Side_Bearing);
    Same(min_Right_Side_Bearing);
    Same(xMax_Extent);
    Same(caret_Slope_Rise);
    Same(caret_Slope_Run);
    Same(caret_Offset);
    SameArray(Reserved);
    Same(metric_Data_Format);
    Same(number_Of_HMetrics);
    Same(long_metrics);
    Same(short_metrics);
    Require(actual.Version == 0x10000 && actual.number_Of_HMetrics == 1);
}

static void Expect(const GuestTtVertHeader& actual, const TT_VertHeader& expected) {
    Same(Version);
    Same(Ascender);
    Same(Descender);
    Same(Line_Gap);
    Same(advance_Height_Max);
    Same(min_Top_Side_Bearing);
    Same(min_Bottom_Side_Bearing);
    Same(yMax_Extent);
    Same(caret_Slope_Rise);
    Same(caret_Slope_Run);
    Same(caret_Offset);
    SameArray(Reserved);
    Same(metric_Data_Format);
    Same(number_Of_VMetrics);
    Same(long_metrics);
    Same(short_metrics);
    Require(actual.Version == 0x10000 && actual.number_Of_VMetrics == 1);
}

static void Expect(const GuestTtPostscript& actual, const TT_Postscript& expected) {
    Same(FormatType);
    Same(italicAngle);
    Same(underlinePosition);
    Same(underlineThickness);
    Same(isFixedPitch);
    Same(minMemType42);
    Same(maxMemType42);
    Same(minMemType1);
    Same(maxMemType1);
    Require(actual.FormatType == 0x30000 && actual.italicAngle == -0x000C8000 && actual.underlinePosition == -100);
    Require(actual.isFixedPitch == 0x80000000u && actual.maxMemType1 == 0xFFFFFFFFu);
}

static void Expect(const GuestTtPclt& actual, const TT_PCLT& expected) {
    Same(Version);
    Same(FontNumber);
    Same(Pitch);
    Same(xHeight);
    Same(Style);
    Same(TypeFamily);
    Same(CapHeight);
    Same(SymbolSet);
    SameArray(TypeFace);
    SameArray(CharacterComplement);
    SameArray(FileName);
    Same(StrokeWeight);
    Same(WidthType);
    Same(SerifStyle);
    Same(Reserved);
    Require(actual.Version == 0x10000 && actual.FontNumber == 0x80000007u && actual.SymbolSet == 0x1234);
    Require(std::memcmp(actual.TypeFace, "AnyPS5 PCLT face", 16) == 0 && actual.StrokeWeight == -3 && actual.Reserved == 0xAB);
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    FacePair(GuestLibrary& library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }

    template <typename TGuest, typename TNative>
    const TGuest* Table(std::int32_t tag, bool present) const {
        auto* rec = const_cast<GuestFaceRec*>(&guest.rec);
        void* actual = FT_Get_Sfnt_Table_nid_postfix(rec, tag);
        void* expected = FT_Get_Sfnt_Table(oracle, static_cast<FT_Sfnt_Tag>(tag));
        Require((actual != nullptr) == (expected != nullptr));
        Require((actual != nullptr) == present);
        Require(FT_Get_Sfnt_Table_nid_postfix(rec, tag) == actual);
        void* native = FT_Get_Sfnt_Table(guest.host, static_cast<FT_Sfnt_Tag>(tag));
        if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) Require(actual == native);
        else Require(actual != native || !actual);
        return static_cast<const TGuest*>(actual);
    }

    template <typename TGuest, typename TNative>
    void Compare(std::int32_t tag) const {
        const TGuest* actual = Table<TGuest, TNative>(tag, true);
        Expect(*actual, *static_cast<const TNative*>(FT_Get_Sfnt_Table(oracle, static_cast<FT_Sfnt_Tag>(tag))));
    }

    void Absent(std::int32_t tag) const { Require((Table<void, void>(tag, false) == nullptr)); }
};

static std::vector<unsigned char> Os2Table() {
    std::vector<unsigned char> os2(100, 0);
    Put16(os2, 0, 5);
    Put16(os2, 2, 0xFFF4);
    os2[8] = 0x01;
    os2[9] = 0x04;
    Put32(os2, 42, 0x80000001u);
    Put32(os2, 46, 0x00000002u);
    Put32(os2, 50, 0x40000000u);
    Put32(os2, 54, 0xFFFFFFFFu);
    std::memcpy(os2.data() + 58, "APS5", 4);
    os2[68] = 0x03;
    Put32(os2, 78, 0x80000000u);
    Put32(os2, 82, 0x00000002u);
    Put16(os2, 86, 0xFF38);
    Put16(os2, 98, 0xFFFF);
    return os2;
}

static std::vector<unsigned char> PostTable() {
    std::vector<unsigned char> post(32, 0);
    Put32(post, 0, 0x00030000u);
    Put32(post, 4, 0xFFF38000u);
    Put16(post, 8, 0xFF9C);
    Put16(post, 10, 50);
    Put32(post, 12, 0x80000000u);
    Put32(post, 16, 1);
    Put32(post, 20, 0x7FFFFFFFu);
    Put32(post, 24, 0x80000001u);
    Put32(post, 28, 0xFFFFFFFFu);
    return post;
}

static std::vector<unsigned char> PcltTable() {
    std::vector<unsigned char> pclt(54, 0);
    Put32(pclt, 0, 0x00010000u);
    Put32(pclt, 4, 0x80000007u);
    Put16(pclt, 8, 500);
    Put16(pclt, 18, 0x1234);
    std::memcpy(pclt.data() + 20, "AnyPS5 PCLT face", 16);
    std::memcpy(pclt.data() + 36, "COMPLEMT", 8);
    std::memcpy(pclt.data() + 44, "APS5FN", 6);
    pclt[50] = 0xFD;
    pclt[51] = 0x02;
    pclt[52] = 0x81;
    pclt[53] = 0xAB;
    return pclt;
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        std::vector<unsigned char> vheaBytes = FontFixture::Words({1, 0, 700, -300, 10, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
        std::vector<unsigned char> vmtx = FontFixture::Words({1000, 0});
        vmtx.resize(4 + 2 * 2);
        FacePair face(library, nativeLibrary,
                      FontFixture::BuildFont(3, {{"OS/2", Os2Table()}, {"post", PostTable()}, {"PCLT", PcltTable()}, {"vhea", vheaBytes}, {"vmtx", vmtx}}));
        face.Compare<GuestTtHeader, TT_Header>(FT_SFNT_HEAD);
        face.Compare<GuestTtMaxProfile, TT_MaxProfile>(FT_SFNT_MAXP);
        face.Compare<GuestTtOs2, TT_OS2>(FT_SFNT_OS2);
        face.Compare<GuestTtPostscript, TT_Postscript>(FT_SFNT_POST);
        face.Compare<GuestTtPclt, TT_PCLT>(FT_SFNT_PCLT);
        face.Compare<GuestTtHoriHeader, TT_HoriHeader>(FT_SFNT_HHEA);
        face.Compare<GuestTtVertHeader, TT_VertHeader>(FT_SFNT_VHEA);
        const auto* hhea = face.Table<GuestTtHoriHeader, TT_HoriHeader>(FT_SFNT_HHEA, true);
        const auto* vhea = face.Table<GuestTtVertHeader, TT_VertHeader>(FT_SFNT_VHEA, true);
        Require(hhea->Ascender == 800 && hhea->Descender == -200 && hhea->advance_Width_Max == 500);
        Require(vhea->Ascender == 700 && vhea->Descender == -300 && vhea->Line_Gap == 10 && vhea->min_Top_Side_Bearing == 1);
        Require(static_cast<const void*>(hhea) != static_cast<const void*>(vhea));
        face.Absent(FT_SFNT_MAX);
        face.Absent(-1);

        FacePair plain(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        plain.Absent(FT_SFNT_VHEA);
        plain.Absent(FT_SFNT_PCLT);
        plain.Absent(FT_SFNT_OS2);
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        bitmap.Absent(FT_SFNT_HEAD);
        bitmap.Absent(FT_SFNT_OS2);
    }
    Require(FT_Get_Sfnt_Table_nid_postfix(nullptr, FT_SFNT_HEAD) == FT_Get_Sfnt_Table(nullptr, FT_SFNT_HEAD));
    GuestFaceRec empty{};
    Require(FT_Get_Sfnt_Table_nid_postfix(&empty, FT_SFNT_HEAD) == nullptr);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
