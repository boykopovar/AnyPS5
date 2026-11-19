#include "prx/libfontconfig/include/Configuration.hpp"
#include "prx/libfontconfig/include/InventoryExports.hpp"
#include "prx/libfontconfig/include/Sets.hpp"
#include "prx/libfontconfig/include/Config.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <array>
#include <bit>
#include <map>
#include <new>
#include <utility>

static bool CopyGuestIn(void* dst, const void* src, std::size_t size) {
    const auto preserved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = preserved;
    return ok;
}

static bool CopyGuestOut(void* dst, const void* src, std::size_t size) {
    const auto preserved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = preserved;
    return ok;
}

static bool IsSubset(const FcCharSet& subset, const FcCharSet& superset) {
    for (const auto& [number, leaf] : subset.leaves) {
        const auto found = superset.leaves.find(number);
        for (size_t word = 0; word < leaf.size(); ++word) {
            const uint32_t covered = found == superset.leaves.end() ? 0 : found->second[word];
            if (leaf[word] & ~covered) return false;
        }
    }
    return true;
}

extern "C" {

FcConfig* APS5_VABI FcConfigGetCurrent() {
    return nullptr;
}

FcFontSet* APS5_VABI FcFontList(FcConfig* config, FcPattern*, FcObjectSet*) {
    if (!config && !FcInitBringUptoDate()) return nullptr;
    if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

FcPattern* APS5_VABI FcFontSetMatch(FcConfig* config, FcFontSet** sets, int, FcPattern* pattern, FcResult* result) {
    if (!sets || !pattern || !result) NotImplemented_nid_no_patch(__func__);
    const FcResult noMatch = FcResultNoMatch;
    if (!CopyGuestOut(result, &noMatch, sizeof(noMatch))) NotImplemented_nid_no_patch(__func__);
    if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

FcBool APS5_VABI FcConfigSubstitute(FcConfig* config, FcPattern*, int kind) {
    if (kind < FcMatchKindBegin || kind >= FcMatchKindEnd) return 0;
    if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI FcInit() {
    return FcInitBringUptoDate();
}

int APS5_VABI FcInitBringUptoDate() {
    return 1;
}

FcPattern* APS5_VABI FcFontMatch(FcConfig* config, FcPattern* pattern, FcResult* result) {
    if (!pattern || !result) NotImplemented_nid_no_patch(__func__);
    const FcResult noMatch = FcResultNoMatch;
    if (!CopyGuestOut(result, &noMatch, sizeof(noMatch))) NotImplemented_nid_no_patch(__func__);
    if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t APS5_VABI FcCharSetSubtractCount(const FcCharSet* first, const FcCharSet* second) {
    if (!first || !second) return 0;
    uint32_t count = 0;
    for (const auto& [number, leaf] : first->leaves) {
        const auto found = second->leaves.find(number);
        for (size_t word = 0; word < leaf.size(); ++word) {
            const uint32_t removed = found == second->leaves.end() ? 0 : found->second[word];
            count += static_cast<uint32_t>(std::popcount(leaf[word] & ~removed));
        }
    }
    return count;
}

FcFontSet* APS5_VABI FcFontSort(FcConfig* config, FcPattern* pattern, FcBool, FcCharSet**, FcResult* result) {
    if (!pattern || !result) NotImplemented_nid_no_patch(__func__);
    const FcResult noMatch = FcResultNoMatch;
    if (!CopyGuestOut(result, &noMatch, sizeof(noMatch))) NotImplemented_nid_no_patch(__func__);
    if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

int APS5_VABI FcCharSetMerge(FcCharSet* target, const FcCharSet* source, int* changed) {
    if (!target || !source) return 0;
    if (changed) {
        const int didChange = !IsSubset(*source, *target);
        if (!CopyGuestOut(changed, &didChange, sizeof(didChange))) return 0;
        if (!didChange) return 1;
    }
    try {
        for (const auto& [number, leaf] : source->leaves) {
            CharSetLeaf& merged = target->leaves[number];
            for (size_t word = 0; word < leaf.size(); ++word) merged[word] |= leaf[word];
        }
    } catch (const std::bad_alloc&) {
        return 0;
    }
    return 1;
}

void APS5_VABI FcCharSetDestroy(FcCharSet* set) {
    if (set && set->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete set;
}

FcCharSet* APS5_VABI FcCharSetCreate() {
    return new (std::nothrow) FcCharSet;
}

std::uint32_t APS5_VABI FcFreeTypeCharIndex(FreeType::GuestFaceRec* face, std::uint32_t character) {
    if (!face) return 0;
    constexpr std::array encodings{FT_ENCODING_UNICODE, FT_ENCODING_MS_SYMBOL};
    const auto current = face->charmap ? face->charmap->encoding : 0;
    const std::size_t initial = current == encodings[1] ? 1 : 0;
    for (std::size_t offset = 0; offset < encodings.size(); ++offset) {
        const auto encoding = encodings[(initial + offset) % encodings.size()];
        if ((!face->charmap || face->charmap->encoding != encoding) &&
            FT_Select_Charmap_nid_postfix(face, encoding) != 0) continue;
        if (const auto glyph = FT_Get_Char_Index_nid_postfix(face, character)) return glyph;
        if (character < 0x100 && encoding == FT_ENCODING_MS_SYMBOL) {
            if (const auto glyph = FT_Get_Char_Index_nid_postfix(face, character + 0xf000)) return glyph;
        }
    }
    return 0;
}

int APS5_VABI FcCharSetAddChar(FcCharSet* set, uint32_t character) {
    if (!set) return 0;
    const uint16_t number = static_cast<uint16_t>(character >> 8);
    if (character > 0x00ffffff && !set->leaves.contains(number)) return 0;
    try {
        CharSetLeaf& leaf = set->leaves[number];
        leaf[(character & 0xff) >> 5] |= uint32_t{1} << (character & 0x1f);
    } catch (const std::bad_alloc&) {
        return 0;
    }
    return 1;
}

void APS5_VABI FcMatrixMultiply(FcMatrix* result, const FcMatrix* first, const FcMatrix* second) {
    if (!result || !first || !second) return;
    FcMatrix a{};
    FcMatrix b{};
    if (!CopyGuestIn(&a, first, sizeof(a)) || !CopyGuestIn(&b, second, sizeof(b))) return;
    const FcMatrix product = {
        a.xx * b.xx + a.xy * b.yx,
        a.xx * b.xy + a.xy * b.yy,
        a.yx * b.xx + a.yy * b.yx,
        a.yx * b.xy + a.yy * b.yy,
    };
    CopyGuestOut(result, &product, sizeof(product));
}

void APS5_VABI FcDefaultSubstitute(FcPattern* pattern) {
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    FcValue value{};
    const auto absent = [&](const char* object) {
        return FcPatternGet_nid_postfix(pattern, object, 0, &value) == FcResultNoMatch;
    };
    const auto number = [&](const char* object, double fallback) {
        if (FcPatternGet_nid_postfix(pattern, object, 0, &value) != FcResultMatch) return fallback;
        if (value.type == FcTypeInteger) return static_cast<double>(value.u.i);
        if (value.type == FcTypeDouble) return value.u.d;
        return fallback;
    };
    if (absent("weight")) FcPatternAddInteger_nid_postfix(pattern, "weight", 80);
    if (absent("slant")) FcPatternAddInteger_nid_postfix(pattern, "slant", 0);
    if (absent("width")) FcPatternAddInteger_nid_postfix(pattern, "width", 100);
    static constexpr std::array<std::pair<const char*, FcBool>, 8> booleans{{
        {"hinting", 1}, {"verticallayout", 0}, {"autohint", 0}, {"globaladvance", 1},
        {"embeddedbitmap", 1}, {"decorative", 0}, {"symbol", 0}, {"variable", 0},
    }};
    for (const auto& [object, fallback] : booleans)
        if (absent(object)) FcPatternAddBool_nid_postfix(pattern, object, fallback);
    double size = number("size", 12.0);
    const double scale = number("scale", 1.0);
    const double dpi = number("dpi", 75.0);
    if (FcPatternGet_nid_postfix(pattern, "pixelsize", 0, &value) != FcResultMatch) {
        FcPatternDel_nid_postfix(pattern, "scale");
        FcPatternAddDouble(pattern, "scale", scale);
        FcPatternDel_nid_postfix(pattern, "dpi");
        FcPatternAddDouble(pattern, "dpi", dpi);
        FcPatternAddDouble(pattern, "pixelsize", size * scale * (dpi / 72.0));
    } else {
        size = number("pixelsize", 0.0) / dpi * 72.0 / scale;
    }
    FcPatternDel_nid_postfix(pattern, "size");
    FcPatternAddDouble(pattern, "size", size);
    if (absent("fontversion")) FcPatternAddInteger_nid_postfix(pattern, "fontversion", 0x7fffffff);
    if (absent("hintstyle")) FcPatternAddInteger_nid_postfix(pattern, "hintstyle", 3);
    if (absent("namelang")) FcPatternAddString(pattern, "namelang", reinterpret_cast<const FcChar8*>("en"));
    FcChar8* found = nullptr;
    if (FcPatternGetString_nid_postfix(pattern, "namelang", 0, &found) == FcResultMatch) {
        for (const char* object : {"familylang", "stylelang", "fullnamelang"}) {
            if (!absent(object)) continue;
            FcPatternAddString(pattern, object, found);
            FcPatternAddString(pattern, object, reinterpret_cast<const FcChar8*>("en-us"));
        }
    }
    if (absent("order")) FcPatternAddInteger_nid_postfix(pattern, "order", 0);
}


}
