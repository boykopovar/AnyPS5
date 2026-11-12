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
    const FcMatrix product = {
        first->xx * second->xx + first->xy * second->yx,
        first->xx * second->xy + first->xy * second->yy,
        first->yx * second->xx + first->yy * second->yx,
        first->yx * second->xy + first->yy * second->yy,
    };
    *result = product;
}

int APS5_VABI FcDefaultSubstitute() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternFormat() {
    NotImplemented_nid_no_patch(__func__);
}

}
