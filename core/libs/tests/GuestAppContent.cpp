#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceAppContentAddcontMount(uint32_t, const NpUnifiedEntitlementLabel*, AppContentMountPoint*);
int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint*);
}

static constexpr int ErrorParameter = static_cast<int>(0x80D90002);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static void Require(bool value) {
    if (!value)
        std::abort();
}

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
}
