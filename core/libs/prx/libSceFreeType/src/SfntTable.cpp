#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <cstdint>
#include <iterator>

using namespace FreeType;

namespace {

static_assert(sizeof(FT_Long) != 8 || sizeof(TT_Header) == sizeof(GuestTtHeader));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_HoriHeader) == sizeof(GuestTtHoriHeader));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_VertHeader) == sizeof(GuestTtVertHeader));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_OS2) == sizeof(GuestTtOs2));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_Postscript) == sizeof(GuestTtPostscript));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_PCLT) == sizeof(GuestTtPclt));
static_assert(sizeof(FT_Long) != 8 || sizeof(TT_MaxProfile) == sizeof(GuestTtMaxProfile));

void Copy(GuestTtHeader& out, const TT_Header& in) {
    out.Table_Version = in.Table_Version;
    out.Font_Revision = in.Font_Revision;
    out.CheckSum_Adjust = in.CheckSum_Adjust;
    out.Magic_Number = in.Magic_Number;
    out.Flags = in.Flags;
    out.Units_Per_EM = in.Units_Per_EM;
    std::copy(std::begin(in.Created), std::end(in.Created), out.Created);
    std::copy(std::begin(in.Modified), std::end(in.Modified), out.Modified);
    out.xMin = in.xMin;
    out.yMin = in.yMin;
    out.xMax = in.xMax;
    out.yMax = in.yMax;
    out.Mac_Style = in.Mac_Style;
    out.Lowest_Rec_PPEM = in.Lowest_Rec_PPEM;
    out.Font_Direction = in.Font_Direction;
    out.Index_To_Loc_Format = in.Index_To_Loc_Format;
    out.Glyph_Data_Format = in.Glyph_Data_Format;
}

void Copy(GuestTtMaxProfile& out, const TT_MaxProfile& in) {
    out.version = in.version;
    out.numGlyphs = in.numGlyphs;
    out.maxPoints = in.maxPoints;
    out.maxContours = in.maxContours;
    out.maxCompositePoints = in.maxCompositePoints;
    out.maxCompositeContours = in.maxCompositeContours;
    out.maxZones = in.maxZones;
    out.maxTwilightPoints = in.maxTwilightPoints;
    out.maxStorage = in.maxStorage;
    out.maxFunctionDefs = in.maxFunctionDefs;
    out.maxInstructionDefs = in.maxInstructionDefs;
    out.maxStackElements = in.maxStackElements;
    out.maxSizeOfInstructions = in.maxSizeOfInstructions;
    out.maxComponentElements = in.maxComponentElements;
    out.maxComponentDepth = in.maxComponentDepth;
}

void Copy(GuestTtOs2& out, const TT_OS2& in) {
    out.version = in.version;
    out.xAvgCharWidth = in.xAvgCharWidth;
    out.usWeightClass = in.usWeightClass;
    out.usWidthClass = in.usWidthClass;
    out.fsType = in.fsType;
    out.ySubscriptXSize = in.ySubscriptXSize;
    out.ySubscriptYSize = in.ySubscriptYSize;
    out.ySubscriptXOffset = in.ySubscriptXOffset;
    out.ySubscriptYOffset = in.ySubscriptYOffset;
    out.ySuperscriptXSize = in.ySuperscriptXSize;
    out.ySuperscriptYSize = in.ySuperscriptYSize;
    out.ySuperscriptXOffset = in.ySuperscriptXOffset;
    out.ySuperscriptYOffset = in.ySuperscriptYOffset;
    out.yStrikeoutSize = in.yStrikeoutSize;
    out.yStrikeoutPosition = in.yStrikeoutPosition;
    out.sFamilyClass = in.sFamilyClass;
    std::copy(std::begin(in.panose), std::end(in.panose), out.panose);
    out.ulUnicodeRange1 = in.ulUnicodeRange1;
    out.ulUnicodeRange2 = in.ulUnicodeRange2;
    out.ulUnicodeRange3 = in.ulUnicodeRange3;
    out.ulUnicodeRange4 = in.ulUnicodeRange4;
    std::copy(std::begin(in.achVendID), std::end(in.achVendID), out.achVendID);
    out.fsSelection = in.fsSelection;
    out.usFirstCharIndex = in.usFirstCharIndex;
    out.usLastCharIndex = in.usLastCharIndex;
    out.sTypoAscender = in.sTypoAscender;
    out.sTypoDescender = in.sTypoDescender;
    out.sTypoLineGap = in.sTypoLineGap;
    out.usWinAscent = in.usWinAscent;
    out.usWinDescent = in.usWinDescent;
    out.ulCodePageRange1 = in.ulCodePageRange1;
    out.ulCodePageRange2 = in.ulCodePageRange2;
    out.sxHeight = in.sxHeight;
    out.sCapHeight = in.sCapHeight;
    out.usDefaultChar = in.usDefaultChar;
    out.usBreakChar = in.usBreakChar;
    out.usMaxContext = in.usMaxContext;
    out.usLowerOpticalPointSize = in.usLowerOpticalPointSize;
    out.usUpperOpticalPointSize = in.usUpperOpticalPointSize;
}

void Copy(GuestTtHoriHeader& out, const TT_HoriHeader& in) {
    out.Version = in.Version;
    out.Ascender = in.Ascender;
    out.Descender = in.Descender;
    out.Line_Gap = in.Line_Gap;
    out.advance_Width_Max = in.advance_Width_Max;
    out.min_Left_Side_Bearing = in.min_Left_Side_Bearing;
    out.min_Right_Side_Bearing = in.min_Right_Side_Bearing;
    out.xMax_Extent = in.xMax_Extent;
    out.caret_Slope_Rise = in.caret_Slope_Rise;
    out.caret_Slope_Run = in.caret_Slope_Run;
    out.caret_Offset = in.caret_Offset;
    std::copy(std::begin(in.Reserved), std::end(in.Reserved), out.Reserved);
    out.metric_Data_Format = in.metric_Data_Format;
    out.number_Of_HMetrics = in.number_Of_HMetrics;
    out.long_metrics = in.long_metrics;
    out.short_metrics = in.short_metrics;
}

void Copy(GuestTtVertHeader& out, const TT_VertHeader& in) {
    out.Version = in.Version;
    out.Ascender = in.Ascender;
    out.Descender = in.Descender;
    out.Line_Gap = in.Line_Gap;
    out.advance_Height_Max = in.advance_Height_Max;
    out.min_Top_Side_Bearing = in.min_Top_Side_Bearing;
    out.min_Bottom_Side_Bearing = in.min_Bottom_Side_Bearing;
    out.yMax_Extent = in.yMax_Extent;
    out.caret_Slope_Rise = in.caret_Slope_Rise;
    out.caret_Slope_Run = in.caret_Slope_Run;
    out.caret_Offset = in.caret_Offset;
    std::copy(std::begin(in.Reserved), std::end(in.Reserved), out.Reserved);
    out.metric_Data_Format = in.metric_Data_Format;
    out.number_Of_VMetrics = in.number_Of_VMetrics;
    out.long_metrics = in.long_metrics;
    out.short_metrics = in.short_metrics;
}

void Copy(GuestTtPostscript& out, const TT_Postscript& in) {
    out.FormatType = in.FormatType;
    out.italicAngle = in.italicAngle;
    out.underlinePosition = in.underlinePosition;
    out.underlineThickness = in.underlineThickness;
    out.isFixedPitch = in.isFixedPitch;
    out.minMemType42 = in.minMemType42;
    out.maxMemType42 = in.maxMemType42;
    out.minMemType1 = in.minMemType1;
    out.maxMemType1 = in.maxMemType1;
}

void Copy(GuestTtPclt& out, const TT_PCLT& in) {
    out.Version = in.Version;
    out.FontNumber = in.FontNumber;
    out.Pitch = in.Pitch;
    out.xHeight = in.xHeight;
    out.Style = in.Style;
    out.TypeFamily = in.TypeFamily;
    out.CapHeight = in.CapHeight;
    out.SymbolSet = in.SymbolSet;
    std::copy(std::begin(in.TypeFace), std::end(in.TypeFace), out.TypeFace);
    std::copy(std::begin(in.CharacterComplement), std::end(in.CharacterComplement), out.CharacterComplement);
    std::copy(std::begin(in.FileName), std::end(in.FileName), out.FileName);
    out.StrokeWeight = in.StrokeWeight;
    out.WidthType = in.WidthType;
    out.SerifStyle = in.SerifStyle;
    out.Reserved = in.Reserved;
}

void* GuestTable(GuestSfntTables& tables, FT_Sfnt_Tag tag, void* native) {
    switch (tag) {
    case FT_SFNT_HEAD:
        Copy(tables.header, *static_cast<const TT_Header*>(native));
        return &tables.header;
    case FT_SFNT_MAXP:
        Copy(tables.maxProfile, *static_cast<const TT_MaxProfile*>(native));
        return &tables.maxProfile;
    case FT_SFNT_OS2:
        Copy(tables.os2, *static_cast<const TT_OS2*>(native));
        return &tables.os2;
    case FT_SFNT_HHEA:
        Copy(tables.horizontal, *static_cast<const TT_HoriHeader*>(native));
        return &tables.horizontal;
    case FT_SFNT_VHEA:
        Copy(tables.vertical, *static_cast<const TT_VertHeader*>(native));
        return &tables.vertical;
    case FT_SFNT_POST:
        Copy(tables.postscript, *static_cast<const TT_Postscript*>(native));
        return &tables.postscript;
    case FT_SFNT_PCLT:
        Copy(tables.pclt, *static_cast<const TT_PCLT*>(native));
        return &tables.pclt;
    default:
        return nullptr;
    }
}

}

extern "C" {

void* APS5_VABI FT_Get_Sfnt_Table_nid_postfix(GuestFaceRec* face, std::int32_t tag) {
    GuestFace* owner = FaceOf(face);
    void* native = FT_Get_Sfnt_Table(owner ? owner->host : nullptr, static_cast<FT_Sfnt_Tag>(tag));
    if constexpr (sizeof(FT_Long) == 8) {
        return native;
    } else {
        return native ? GuestTable(owner->sfntTables, static_cast<FT_Sfnt_Tag>(tag), native) : nullptr;
    }
}

}
