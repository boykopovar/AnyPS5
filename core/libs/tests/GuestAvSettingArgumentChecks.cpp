#include "prx/libSceAvSetting/libSceAvSetting.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int Error0002 = static_cast<int>(0x802A0002u);
constexpr int Error0007 = static_cast<int>(0x802A0007u);
constexpr int NotInitialized = static_cast<int>(0x802A001Au);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void CheckDeviceConnected() {
    for (std::int32_t device : {0, 2, 3, -1}) Require(sceAvControlIsDeviceConnected(device) == Error0002, "IsDeviceConnected rejects a device other than 1");
    Require(sceAvControlIsDeviceConnected(1) == NotInitialized, "IsDeviceConnected(1) answers 0x802A001A before initialization");
}

void CheckProcessAttribute() {
    using Change = decltype(&sceAvControlChangeProcessAttribute);
    for (Change change : {sceAvControlChangeProcessAttribute, sceAvSettingChangeProcessAttribute}) {
        for (std::uint32_t attribute : {0x20u, 0x80u, 0x4000u, 0x2000000u}) {
            Require(change(attribute, 0) == NotInitialized, "an accepted attribute with flag 0 answers 0x802A001A");
            Require(change(attribute, 1) == NotInitialized, "an accepted attribute with flag 1 answers 0x802A001A");
            Require(change(attribute, 2) == Error0002, "a flag above 1 is rejected");
            Require(change(attribute, 0xffffffffu) == Error0002, "a flag of 0xffffffff is rejected");
        }
        for (std::uint32_t attribute : {0u, 1u, 0x40u, 0x3fffu, 0x4001u, 0x1000000u, 0xffffffffu}) Require(change(attribute, 0) == Error0002, "another attribute is rejected");
    }
}

void CheckMetadata() {
    Require(sceAvControlSetHdrMetadata(0x7000) == NotInitialized, "SetHdrMetadata(0x7000) answers 0x802A001A");
    Require(libSceAvSettingUnknown_MNZaRdvvdU4_nid_no_patch(0x7005) == NotInitialized, "MNZaRdvvdU4(0x7005) answers 0x802A001A");
    for (std::uint64_t type : {std::uint64_t{0}, std::uint64_t{0x7005}, std::uint64_t{0x100007000}, ~std::uint64_t{0}}) Require(sceAvControlSetHdrMetadata(type) == Error0007, "SetHdrMetadata rejects another type");
    for (std::uint64_t type : {std::uint64_t{0}, std::uint64_t{0x7000}, std::uint64_t{0x100007005}, ~std::uint64_t{0}}) Require(libSceAvSettingUnknown_MNZaRdvvdU4_nid_no_patch(type) == Error0007, "MNZaRdvvdU4 rejects another type");
}

void CheckPointerCalls() {
    std::uint32_t block[16] = {};
    Require(libSceAvSettingUnknown_0BKuKlQG8zE_nid_no_patch(1, block) == NotInitialized, "0BKuKlQG8zE(1, block) answers 0x802A001A");
    Require(libSceAvSettingUnknown_0BKuKlQG8zE_nid_no_patch(1, nullptr) == Error0002, "0BKuKlQG8zE rejects a null block");
    Require(libSceAvSettingUnknown_0BKuKlQG8zE_nid_no_patch(0, block) == Error0002, "0BKuKlQG8zE rejects a kind other than 1");
    Require(libSceAvSettingUnknown_0BKuKlQG8zE_nid_no_patch(2, block) == Error0002, "0BKuKlQG8zE rejects kind 2");
    unsigned char info[64];
    for (unsigned char& b : info) b = 0xA5;
    Require(sceAvSettingGetCurrentDeviceInfo_(0, info) == NotInitialized, "GetCurrentDeviceInfo_(0, info) answers 0x802A001A");
    bool untouched = true;
    for (unsigned char b : info) untouched = untouched && b == 0xA5;
    Require(untouched, "GetCurrentDeviceInfo_ writes nothing before initialization");
    Require(sceAvSettingGetCurrentDeviceInfo_(0, nullptr) == Error0002, "GetCurrentDeviceInfo_ rejects a null info");
    Require(sceAvSettingGetCurrentDeviceInfo_(1, info) == Error0002, "GetCurrentDeviceInfo_ rejects a device other than 0");
    Require(sceAvSettingGetCurrentDeviceInfo_(-1, info) == Error0002, "GetCurrentDeviceInfo_ rejects device -1");
}

void CheckSimulate() {
    for (std::uint32_t argument1 : {0u, 1u, 5u, 0xffffffffu}) {
        Require(sceAvSettingSimulateProcessOutputModeArbitration_(argument1, 0xffffffffu) == NotInitialized, "Simulate with 0xffffffff answers 0x802A001A");
        Require(sceAvSettingSimulateProcessOutputModeArbitration_(argument1, 0) == Error0002, "Simulate rejects 0");
        Require(sceAvSettingSimulateProcessOutputModeArbitration_(argument1, 0xfffffffeu) == Error0002, "Simulate rejects 0xfffffffe");
    }
}

} // namespace

int main() {
    CheckDeviceConnected();
    CheckProcessAttribute();
    CheckMetadata();
    CheckPointerCalls();
    CheckSimulate();
    return 0;
}
