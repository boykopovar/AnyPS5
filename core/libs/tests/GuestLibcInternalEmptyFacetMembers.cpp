#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibcInternal empty facet members: %s\n", message);
        std::abort();
    }
}

}

int main() {
    unsigned char object[0x80];
    unsigned char locinfo[0x80];
    std::memset(object, 0xa5, sizeof(object));
    std::memset(locinfo, 0x5a, sizeof(locinfo));
    unsigned char untouchedObject[sizeof(object)];
    unsigned char untouchedLocinfo[sizeof(locinfo)];
    std::memcpy(untouchedObject, object, sizeof(object));
    std::memcpy(untouchedLocinfo, locinfo, sizeof(locinfo));
    libSceLibcInternalInventory_021_nid_no_patch(object);
    libSceLibcInternalInventory_022_nid_no_patch(object);
    libSceLibcInternalInventory_091_nid_no_patch(object);
    libSceLibcInternalInventory_097_nid_no_patch(object);
    libSceLibcInternalInventory_098_nid_no_patch(object);
    libSceLibcInternalInventory_102_nid_no_patch(object);
    libSceLibcInternalInventory_103_nid_no_patch(object);
    libSceLibcInternalInventory_111_nid_no_patch(object);
    libSceLibcInternalInventory_112_nid_no_patch(object);
    libSceLibcInternalInventory_117_nid_no_patch(object);
    libSceLibcInternalInventory_118_nid_no_patch(object);
    libSceLibcInternalInventory_119_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_125_nid_no_patch(object);
    libSceLibcInternalInventory_126_nid_no_patch(object);
    libSceLibcInternalInventory_127_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_132_nid_no_patch(object);
    libSceLibcInternalInventory_137_nid_no_patch(object);
    libSceLibcInternalInventory_138_nid_no_patch(object);
    libSceLibcInternalInventory_149_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_153_nid_no_patch(object);
    libSceLibcInternalInventory_154_nid_no_patch(object);
    libSceLibcInternalInventory_159_nid_no_patch(object);
    libSceLibcInternalInventory_160_nid_no_patch(object);
    libSceLibcInternalInventory_162_nid_no_patch(object);
    libSceLibcInternalInventory_168_nid_no_patch(object);
    libSceLibcInternalInventory_178_nid_no_patch(object);
    libSceLibcInternalInventory_185_nid_no_patch(object);
    libSceLibcInternalInventory_202_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_209_nid_no_patch(object);
    libSceLibcInternalInventory_210_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_213_nid_no_patch(object);
    libSceLibcInternalInventory_214_nid_no_patch(object);
    libSceLibcInternalInventory_217_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_224_nid_no_patch(object);
    libSceLibcInternalInventory_225_nid_no_patch(object);
    libSceLibcInternalInventory_227_nid_no_patch(object, locinfo);
    libSceLibcInternalInventory_231_nid_no_patch(object);
    libSceLibcInternalInventory_233_nid_no_patch(object);
    libSceLibcInternalInventory_234_nid_no_patch(object);
    sceLibcInternalUnknown6_004_V2FICbvPaps_nid_no_patch(object, locinfo);
    sceLibcInternalUnknown6_007_hnGhTkIDFHg_nid_no_patch(object);
    sceLibcInternalUnknown6_026_U4JPmRpm70c_nid_no_patch(object);
    sceLibcInternalUnknown6_029_ZzmRfDtowlo_nid_no_patch(object, locinfo);
    sceLibcInternalUnknown6_034_vu0BpBGlol4_nid_no_patch(object);
    sceLibcInternalUnknown6_041_to7GggwECZU_nid_no_patch(object);
    sceLibcInternalUnknown6_061_8PJ9qmklj2c_nid_no_patch(object);
    sceLibcInternalUnknown6_071_CiD6mBPDZrA_nid_no_patch(object);
    sceLibcInternalUnknown6_083_hpc9OSfCAEg_nid_no_patch(object);
    sceLibcInternalUnknown6_090_Oeo9tUbzW7s_nid_no_patch(object);
    sceLibcInternalUnknown6_140_iZCHNahjpp4_nid_no_patch(object, locinfo);
    sceLibcInternalUnknown6_141_zkCx9c2QKBc_nid_no_patch(object);
    sceLibcInternalUnknown6_150_cXLpLN0lSwc_nid_no_patch(object, locinfo);
    Require(std::memcmp(object, untouchedObject, sizeof(object)) == 0, "an empty member wrote the object");
    Require(std::memcmp(locinfo, untouchedLocinfo, sizeof(locinfo)) == 0, "an _Init member wrote the _Locinfo");
    return 0;
}
