#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTSFNTTABLES_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTSFNTTABLES_HPP

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestTtHeader {
    std::int64_t Table_Version;
    std::int64_t Font_Revision;
    std::int64_t CheckSum_Adjust;
    std::int64_t Magic_Number;
    std::uint16_t Flags;
    std::uint16_t Units_Per_EM;
    std::uint64_t Created[2];
    std::uint64_t Modified[2];
    std::int16_t xMin;
    std::int16_t yMin;
    std::int16_t xMax;
    std::int16_t yMax;
    std::uint16_t Mac_Style;
    std::uint16_t Lowest_Rec_PPEM;
    std::int16_t Font_Direction;
    std::int16_t Index_To_Loc_Format;
    std::int16_t Glyph_Data_Format;
};

struct GuestTtHoriHeader {
    std::int64_t Version;
    std::int16_t Ascender;
    std::int16_t Descender;
    std::int16_t Line_Gap;
    std::uint16_t advance_Width_Max;
    std::int16_t min_Left_Side_Bearing;
    std::int16_t min_Right_Side_Bearing;
    std::int16_t xMax_Extent;
    std::int16_t caret_Slope_Rise;
    std::int16_t caret_Slope_Run;
    std::int16_t caret_Offset;
    std::int16_t Reserved[4];
    std::int16_t metric_Data_Format;
    std::uint16_t number_Of_HMetrics;
    void* long_metrics;
    void* short_metrics;
};

struct GuestTtVertHeader {
    std::int64_t Version;
    std::int16_t Ascender;
    std::int16_t Descender;
    std::int16_t Line_Gap;
    std::uint16_t advance_Height_Max;
    std::int16_t min_Top_Side_Bearing;
    std::int16_t min_Bottom_Side_Bearing;
    std::int16_t yMax_Extent;
    std::int16_t caret_Slope_Rise;
    std::int16_t caret_Slope_Run;
    std::int16_t caret_Offset;
    std::int16_t Reserved[4];
    std::int16_t metric_Data_Format;
    std::uint16_t number_Of_VMetrics;
    void* long_metrics;
    void* short_metrics;
};

struct GuestTtOs2 {
    std::uint16_t version;
    std::int16_t xAvgCharWidth;
    std::uint16_t usWeightClass;
    std::uint16_t usWidthClass;
    std::uint16_t fsType;
    std::int16_t ySubscriptXSize;
    std::int16_t ySubscriptYSize;
    std::int16_t ySubscriptXOffset;
    std::int16_t ySubscriptYOffset;
    std::int16_t ySuperscriptXSize;
    std::int16_t ySuperscriptYSize;
    std::int16_t ySuperscriptXOffset;
    std::int16_t ySuperscriptYOffset;
    std::int16_t yStrikeoutSize;
    std::int16_t yStrikeoutPosition;
    std::int16_t sFamilyClass;
    std::uint8_t panose[10];
    std::uint64_t ulUnicodeRange1;
    std::uint64_t ulUnicodeRange2;
    std::uint64_t ulUnicodeRange3;
    std::uint64_t ulUnicodeRange4;
    char achVendID[4];
    std::uint16_t fsSelection;
    std::uint16_t usFirstCharIndex;
    std::uint16_t usLastCharIndex;
    std::int16_t sTypoAscender;
    std::int16_t sTypoDescender;
    std::int16_t sTypoLineGap;
    std::uint16_t usWinAscent;
    std::uint16_t usWinDescent;
    std::uint64_t ulCodePageRange1;
    std::uint64_t ulCodePageRange2;
    std::int16_t sxHeight;
    std::int16_t sCapHeight;
    std::uint16_t usDefaultChar;
    std::uint16_t usBreakChar;
    std::uint16_t usMaxContext;
    std::uint16_t usLowerOpticalPointSize;
    std::uint16_t usUpperOpticalPointSize;
};

struct GuestTtPostscript {
    std::int64_t FormatType;
    std::int64_t italicAngle;
    std::int16_t underlinePosition;
    std::int16_t underlineThickness;
    std::uint64_t isFixedPitch;
    std::uint64_t minMemType42;
    std::uint64_t maxMemType42;
    std::uint64_t minMemType1;
    std::uint64_t maxMemType1;
};

struct GuestTtPclt {
    std::int64_t Version;
    std::uint64_t FontNumber;
    std::uint16_t Pitch;
    std::uint16_t xHeight;
    std::uint16_t Style;
    std::uint16_t TypeFamily;
    std::uint16_t CapHeight;
    std::uint16_t SymbolSet;
    char TypeFace[16];
    char CharacterComplement[8];
    char FileName[6];
    std::int8_t StrokeWeight;
    std::int8_t WidthType;
    std::uint8_t SerifStyle;
    std::uint8_t Reserved;
};

struct GuestTtMaxProfile {
    std::int64_t version;
    std::uint16_t numGlyphs;
    std::uint16_t maxPoints;
    std::uint16_t maxContours;
    std::uint16_t maxCompositePoints;
    std::uint16_t maxCompositeContours;
    std::uint16_t maxZones;
    std::uint16_t maxTwilightPoints;
    std::uint16_t maxStorage;
    std::uint16_t maxFunctionDefs;
    std::uint16_t maxInstructionDefs;
    std::uint16_t maxStackElements;
    std::uint16_t maxSizeOfInstructions;
    std::uint16_t maxComponentElements;
    std::uint16_t maxComponentDepth;
};

struct GuestSfntTables {
    GuestTtHeader header{};
    GuestTtMaxProfile maxProfile{};
    GuestTtOs2 os2{};
    GuestTtHoriHeader horizontal{};
    GuestTtVertHeader vertical{};
    GuestTtPostscript postscript{};
    GuestTtPclt pclt{};
};

static_assert(sizeof(GuestTtHeader) == 96 && offsetof(GuestTtHeader, Created) == 40 && offsetof(GuestTtHeader, xMin) == 72);
static_assert(sizeof(GuestTtHoriHeader) == 56 && offsetof(GuestTtHoriHeader, Reserved) == 28 && offsetof(GuestTtHoriHeader, long_metrics) == 40);
static_assert(sizeof(GuestTtVertHeader) == 56 && offsetof(GuestTtVertHeader, Reserved) == 28 && offsetof(GuestTtVertHeader, long_metrics) == 40);
static_assert(sizeof(GuestTtOs2) == 136 && offsetof(GuestTtOs2, ulUnicodeRange1) == 48 && offsetof(GuestTtOs2, achVendID) == 80);
static_assert(offsetof(GuestTtOs2, ulCodePageRange1) == 104 && offsetof(GuestTtOs2, sxHeight) == 120);
static_assert(sizeof(GuestTtPostscript) == 64 && offsetof(GuestTtPostscript, isFixedPitch) == 24);
static_assert(sizeof(GuestTtPclt) == 64 && offsetof(GuestTtPclt, TypeFace) == 28 && offsetof(GuestTtPclt, Reserved) == 61);
static_assert(sizeof(GuestTtMaxProfile) == 40 && offsetof(GuestTtMaxProfile, numGlyphs) == 8);

}

#endif
