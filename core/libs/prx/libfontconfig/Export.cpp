#include "prx/libfontconfig/include/Configuration.hpp"
#include "prx/libfontconfig/include/InventoryExports.hpp"
#include "prx/libfontconfig/include/Sets.hpp"
#include "prx/libfontconfig/include/Config.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <array>
#include <bit>
#include <map>
#include <new>
#include <string>
#include <utility>

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

int APS5_VABI FcFontList() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontSetMatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcConfigSubstitute() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcInitBringUptoDate() {
    return 1;
}

int APS5_VABI FcFontRenderPrepare() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontMatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternFilter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetFTFace() {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI FcFontSort() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcCharSetMerge(FcCharSet* target, const FcCharSet* source, int* changed) {
    if (!target || !source) return 0;
    if (changed) {
        *changed = !IsSubset(*source, *target);
        if (!*changed) return 1;
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

int APS5_VABI FcPatternGetCharSet() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI FcCharSetDestroy(FcCharSet* set) {
    delete set;
}

FcCharSet* APS5_VABI FcCharSetCreate() {
    return new (std::nothrow) FcCharSet;
}

int APS5_VABI FcFreeTypeCharIndex() {
    NotImplemented_nid_no_patch(__func__);
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
    const FcMatrix product = {
        first->xx * second->xx + first->xy * second->yx,
        first->xx * second->xy + first->xy * second->yy,
        first->yx * second->xx + first->yy * second->yx,
        first->yx * second->xy + first->yy * second->yy,
    };
    *result = product;
}

int APS5_VABI FcPatternAddCharSet() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI FcDefaultSubstitute(FcPattern* pattern) {
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    FcValue value{};
    const auto absent = [&](const char* object) {
        return FcPatternGet(pattern, object, 0, &value) == FcResultNoMatch;
    };
    const auto number = [&](const char* object, double fallback) {
        if (FcPatternGet(pattern, object, 0, &value) != FcResultMatch) return fallback;
        if (value.type == FcTypeInteger) return static_cast<double>(value.u.i);
        if (value.type == FcTypeDouble) return value.u.d;
        return fallback;
    };
    if (absent("weight")) FcPatternAddInteger(pattern, "weight", 80);
    if (absent("slant")) FcPatternAddInteger(pattern, "slant", 0);
    if (absent("width")) FcPatternAddInteger(pattern, "width", 100);
    static constexpr std::array<std::pair<const char*, FcBool>, 8> booleans{{
        {"hinting", 1}, {"verticallayout", 0}, {"autohint", 0}, {"globaladvance", 1},
        {"embeddedbitmap", 1}, {"decorative", 0}, {"symbol", 0}, {"variable", 0},
    }};
    for (const auto& [object, fallback] : booleans)
        if (absent(object)) FcPatternAddBool(pattern, object, fallback);
    double size = number("size", 12.0);
    const double scale = number("scale", 1.0);
    const double dpi = number("dpi", 75.0);
    if (FcPatternGet(pattern, "pixelsize", 0, &value) != FcResultMatch) {
        FcPatternDel(pattern, "scale");
        FcPatternAddDouble(pattern, "scale", scale);
        FcPatternDel(pattern, "dpi");
        FcPatternAddDouble(pattern, "dpi", dpi);
        FcPatternAddDouble(pattern, "pixelsize", size * scale * (dpi / 72.0));
    } else {
        size = number("pixelsize", 0.0) / dpi * 72.0 / scale;
    }
    FcPatternDel(pattern, "size");
    FcPatternAddDouble(pattern, "size", size);
    if (absent("fontversion")) FcPatternAddInteger(pattern, "fontversion", 0x7fffffff);
    if (absent("hintstyle")) FcPatternAddInteger(pattern, "hintstyle", 3);
    if (absent("namelang")) FcPatternAddString(pattern, "namelang", reinterpret_cast<const FcChar8*>("en"));
    FcChar8* found = nullptr;
    if (FcPatternGetString(pattern, "namelang", 0, &found) == FcResultMatch) {
        const std::string namelang(reinterpret_cast<const char*>(found));
        for (const char* object : {"familylang", "stylelang", "fullnamelang"}) {
            if (!absent(object)) continue;
            FcPatternAddString(pattern, object, reinterpret_cast<const FcChar8*>(namelang.c_str()));
            FcPatternAddString(pattern, object, reinterpret_cast<const FcChar8*>("en-us"));
        }
    }
    if (absent("order")) FcPatternAddInteger(pattern, "order", 0);
}

int APS5_VABI FcPatternAddLangSet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetLangSet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternFormat() {
    NotImplemented_nid_no_patch(__func__);
}

}
