#include "prx/libSceAbstractTwitter/libSceAbstractTwitter.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notSupported = static_cast<int>(0x809E0005u);
    for (int call = 0; call < 2; ++call) {
        Require(_ZNK15AbstractStorage14TwitterContent13GetCapabilityEv() == 0x6C0A);
        Require(_ZNK15AbstractStorage14TwitterContent10GetServiceEv() == 3);
        Require(_ZNK15AbstractStorage13TwitterFolder10GetServiceEv() == 3);
        Require(_ZNK15AbstractStorage14TwitterService14GetServiceTypeEv() == 3);
        Require(_ZNK15AbstractStorage14TwitterStorage10GetServiceEv() == 3);
        Require(_ZNK15AbstractStorage14TwitterContent7GetStatEPNS_4StatE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterContent4ReadEPvlPl() == notSupported);
        Require(_ZN15AbstractStorage14TwitterContent8SeekTimeElPl() == notSupported);
        Require(_ZN15AbstractStorage14TwitterContent5FlushEv() == notSupported);
        Require(_ZN15AbstractStorage14TwitterContent6RemoveEv() == notSupported);
        Require(_ZNK15AbstractStorage13TwitterFolder7GetStatEPNS_4StatE() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder4OpenENS_8ItemTypeE() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder5CloseEv() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder6RemoveEb() == notSupported);
        Require(_ZNK15AbstractStorage14TwitterStorage7GetStatEPNS_4StatE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage14GetStorageSizeEPlS1_S1_() == notSupported);
    }
    return 0;
}
