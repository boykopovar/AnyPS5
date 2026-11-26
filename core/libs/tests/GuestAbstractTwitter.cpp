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
        Require(_ZN15AbstractStorage13TwitterFolder12RemoveFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder13RemoveContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder8NextItemEPSt10shared_ptrINS_4ItemEE() == notSupported);
        Require(_ZN15AbstractStorage13TwitterFolder9NewFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterContent6RenameERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage10GetContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_7ContentEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage11DeserializeESt10shared_ptrINS_7ContentEEPS1_INS_4ItemEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage13GetRootFolderEPSt10shared_ptrINS_6FolderEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage15CreateRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEENS_8ItemTypeEPSt10shared_ptrINS_4ItemEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage15RemoveRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() == notSupported);
        Require(_ZN15AbstractStorage14TwitterStorage7IsExistERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() == 0);
        Require(_ZN15AbstractStorage14TwitterStorage9SerializeESt10shared_ptrINS_7ContentEES1_INS_4ItemEE() == notSupported);
    }
    return 0;
}
