#include "prx/libfontconfig/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <array>
#include <bit>
#include <map>
#include <new>

using CharSetLeaf = std::array<uint32_t, 8>;

struct FcCharSet {
    std::map<uint16_t, CharSetLeaf> leaves;
};

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

int APS5_VABI FcLangSetDestroy() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternRemove() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetInteger() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcConfigGetCurrent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetBool() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternAddBool() {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI FcPatternGetMatrix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcInitBringUptoDate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetString() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontRenderPrepare() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternAddDouble() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternDuplicate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontMatch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternFilter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcConfigCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetFTFace() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternEqual() {
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

int APS5_VABI FcPatternAddInteger() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcLangSetAdd() {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI FcPatternDel() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcConfigDestroy() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternReference() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcObjectSetDestroy() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternAddMatrix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternAddString() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontSetCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcLangSetHasLang() {
    NotImplemented_nid_no_patch(__func__);
}

FcCharSet* APS5_VABI FcCharSetCreate() {
    return new (std::nothrow) FcCharSet;
}

int APS5_VABI FcLangSetCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFreeTypeCharIndex() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontSetAdd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternDestroy() {
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

int APS5_VABI FcDefaultSubstitute() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcObjectSetBuild() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternAddLangSet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcFontSetDestroy() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternGetLangSet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcConfigReference() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FcPatternFormat() {
    NotImplemented_nid_no_patch(__func__);
}

}
