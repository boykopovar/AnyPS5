#include "prx/libSceOpusDec/include/Api.hpp"
#include "prx/libSceOpusDec/include/InventoryExports.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

constexpr int Base = static_cast<int>(0x81740000u);

using Decode6 = decltype(&libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch);
using Decode5 = decltype(&libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch);

static void CheckDecode6(Decode6 decode, const char* name, void* p) {
    Require(decode(nullptr, p, 1, p, 1, 40) == Base, name);
    Require(decode(p, nullptr, 1, p, 1, 40) == Base + 6, name);
    for (const std::uint32_t bytes : {0x1f4cu, 0xffffffffu}) Require(decode(p, p, bytes, p, 1, 40) == Base + 7, name);
    Require(decode(p, p, 1, nullptr, 1, 40) == Base + 8, name);
    for (const std::int32_t count : {0, -1}) Require(decode(p, p, 1, p, count, 40) == Base + 9, name);
    for (const std::int32_t priority : {19, 101, 0, -1}) Require(decode(p, p, 1, p, 1, priority) == Base + 10, name);
    Require(decode(nullptr, nullptr, 0xffffffffu, nullptr, 0, 0) == Base, name);
    Require(decode(p, nullptr, 0xffffffffu, nullptr, 0, 0) == Base + 6, name);
    Require(decode(p, p, 0xffffffffu, nullptr, 0, 0) == Base + 7, name);
    Require(decode(p, p, 1, nullptr, 0, 0) == Base + 8, name);
    Require(decode(p, p, 1, p, 0, 0) == Base + 9, name);
}

static void CheckDecode5(Decode5 decode, const char* name, void* p) {
    Require(decode(nullptr, p, 1, p, 1) == Base, name);
    Require(decode(p, nullptr, 1, p, 1) == Base + 6, name);
    for (const std::uint32_t bytes : {0x1f4cu, 0xffffffffu}) Require(decode(p, p, bytes, p, 1) == Base + 7, name);
    Require(decode(p, p, 1, nullptr, 1) == Base + 8, name);
    for (const std::int32_t count : {0, -1}) Require(decode(p, p, 1, p, count) == Base + 9, name);
}

int main() {
    unsigned char block[0x400]{};
    void* p = block;

    Require(sceOpusDecCreate(nullptr, 1, 0) == Base, "Create: null state first");
    for (const std::int32_t rate : {0, 7999, 8001, 44100, 96000, -1}) Require(sceOpusDecCreate(p, rate, 1) == Base + 2, "Create: rate");
    for (const std::int32_t channels : {0, 3, -1}) Require(sceOpusDecCreate(p, 48000, channels) == Base + 3, "Create: channels");
    Require(sceOpusDecCancel(nullptr) == Base, "Cancel: null state");
    Require(libSceOpusDecUnknown_EF6FftzF33M_nid_no_patch(nullptr) == Base, "EF6FftzF33M: null state");
    Require(libSceOpusDecUnknown_lFTdHt_minus_AuKU_nid_no_patch(nullptr) == Base, "lFTdHt-AuKU: null state");
    Require(libSceOpusDecUnknown_ygCoF49g3F0_nid_no_patch(nullptr) == Base, "ygCoF49g3F0: null state");

    CheckDecode6(libSceOpusDecUnknown__minus_NcPFjTj5zU_nid_no_patch, "-NcPFjTj5zU", p);
    CheckDecode6(libSceOpusDecUnknown_I1ugumHd0_plus_A_nid_no_patch, "I1ugumHd0+A", p);
    CheckDecode6(libSceOpusDecUnknown_ZBx02KtHz2Y_nid_no_patch, "ZBx02KtHz2Y", p);
    CheckDecode6(libSceOpusDecUnknown_eJQx7Emrwb0_nid_no_patch, "eJQx7Emrwb0", p);
    CheckDecode5(libSceOpusDecUnknown_VsujeTF0_plus_xk_nid_no_patch, "VsujeTF0+xk", p);
    CheckDecode5(libSceOpusDecUnknown_ct42RpB2CaM_nid_no_patch, "ct42RpB2CaM", p);

    const auto multi = libSceOpusDecUnknown_JZapTsLx0j0_nid_no_patch;
    Require(multi(nullptr, p, 48000, 1, 0, 1, 0) == Base, "JZapTsLx0j0: null first argument");
    Require(multi(p, nullptr, 48000, 1, 0, 1, 0) == Base, "JZapTsLx0j0: null state");
    Require(multi(p, p, 44100, 1, 0, 1, 0) == Base + 2, "JZapTsLx0j0: rate");
    for (const std::uint32_t streams : {0u, 13u, 0xffffffffu}) Require(multi(p, p, 48000, streams, 0, 1, 0) == Base + 3, "JZapTsLx0j0: streams");
    for (const std::uint32_t coupled : {0u, 13u}) Require(multi(p, p, 48000, 1, 0, coupled, 0) == Base + 0x16, "JZapTsLx0j0: argument 6");
    Require(multi(p, p, 48000, 2, 0, 2, 3) == Base + 0x17, "JZapTsLx0j0: argument 7 above argument 6");
    Require(multi(p, p, 48000, 2, 0, 8, 5) == Base + 3, "JZapTsLx0j0: arguments 6 and 7 above 12");

    const auto multiDefault = libSceOpusDecUnknown_sr25irrwGBI_nid_no_patch;
    Require(multiDefault(nullptr, 48000, 1, 0, 1, 0) == Base, "sr25irrwGBI: null state");
    Require(multiDefault(p, 44100, 1, 0, 1, 0) == Base + 2, "sr25irrwGBI: rate");
    for (const std::uint32_t streams : {0u, 13u}) Require(multiDefault(p, 48000, streams, 0, 1, 0) == Base + 3, "sr25irrwGBI: streams");
    for (const std::uint32_t coupled : {0u, 13u}) Require(multiDefault(p, 48000, 1, 0, coupled, 0) == Base + 0x16, "sr25irrwGBI: argument 5");
    Require(multiDefault(p, 48000, 2, 0, 2, 3) == Base + 0x17, "sr25irrwGBI: argument 6 above argument 5");
    Require(multiDefault(p, 48000, 2, 0, 8, 5) == Base + 3, "sr25irrwGBI: arguments 5 and 6 above 12");
    return 0;
}
