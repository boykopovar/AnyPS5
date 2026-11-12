#include "prx/libSceAbstractLocal/libSceAbstractLocal.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    for (int call = 0; call < 2; ++call) {
        Require(_ZNK15AbstractStorage12LocalContent13GetCapabilityEv() == 0x37C5A);
        Require(_ZNK15AbstractStorage12LocalContent10GetServiceEv() == 0);
        Require(_ZN15AbstractStorage12LocalContent8SeekTimeElPl() == static_cast<int>(0x809E0005u));
        Require(_ZNK15AbstractStorage11LocalFolder10GetServiceEv() == 0);
        Require(_ZNK15AbstractStorage12LocalService14GetServiceTypeEv() == 0);
        Require(_ZNK15AbstractStorage12LocalStorage7GetStatEPNS_4StatE() == static_cast<int>(0x809E0002u));
        Require(_ZNK15AbstractStorage12LocalStorage10GetServiceEv() == 0);
        Require(_ZN15AbstractStorage12LocalStorage14GetStorageSizeEPlS1_S1_() == static_cast<int>(0x809E0002u));
    }
    return 0;
}
