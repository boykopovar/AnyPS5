#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceAppContentAddcontMount(uint32_t, const NpUnifiedEntitlementLabel*, AppContentMountPoint*);
int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint*);
int APS5_VABI sceAppContentGetAddcontInfo(uint32_t, const NpUnifiedEntitlementLabel*, void*);
int APS5_VABI sceAppContentGetAddcontInfoList(uint32_t, void*, uint32_t, uint32_t*);
}

static constexpr int ErrorParameter = static_cast<int>(0x80D90002);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static constexpr int ErrorDrmNoEntitlement = static_cast<int>(0x80D90007);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    NpUnifiedEntitlementLabel label{};
    std::memcpy(&label, "ADDCONT000000001", 16);
    AppContentMountPoint mountPoint{};
    std::memset(&mountPoint, 0x5a, sizeof(mountPoint));
    const AppContentMountPoint untouched = mountPoint;
    Require(sceAppContentAddcontMount(0, &label, &mountPoint) == ErrorNotFound);
    Require(std::memcmp(&mountPoint, &untouched, sizeof(mountPoint)) == 0);
    Require(sceAppContentAddcontMount(0, nullptr, &mountPoint) == ErrorParameter);
    Require(sceAppContentAddcontMount(0, &label, nullptr) == ErrorParameter);
    std::memcpy(mountPoint.data, "/addcont0", 10);
    Require(sceAppContentAddcontUnmount(&mountPoint) == ErrorNotFound);
    Require(sceAppContentAddcontUnmount(nullptr) == ErrorParameter);

    unsigned char info[24];
    std::memset(info, 0x5a, sizeof(info));
    unsigned char untouchedInfo[24];
    std::memcpy(untouchedInfo, info, sizeof(info));
    Require(sceAppContentGetAddcontInfo(0, &label, info) == ErrorDrmNoEntitlement);
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0);
    Require(sceAppContentGetAddcontInfo(0, nullptr, info) == ErrorParameter);
    Require(sceAppContentGetAddcontInfo(0, &label, nullptr) == ErrorParameter);

    uint32_t hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, info, 0, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 1, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, info, 1, &hitNum) == 0);
    Require(hitNum == 0);
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0);
    Require(sceAppContentGetAddcontInfoList(0, info, 1, nullptr) == 0);
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, nullptr) == ErrorParameter);
    Require(sceAppContentGetAddcontInfoList(0, info, 0, nullptr) == ErrorParameter);
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 1, nullptr) == ErrorParameter);
}
