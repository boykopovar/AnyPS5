#include "prx/libSceAbstractStorage/libSceAbstractStorage.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notSupported = static_cast<int>(0x809E0005u);
    constexpr int invalidArgument = static_cast<int>(0x809E0002u);
    for (int call = 0; call < 2; ++call) {
        Require(_ZN15AbstractStorage12Serializable12readExternalESt10shared_ptrINS_7ContentEE() == notSupported);
        Require(_ZN15AbstractStorage12Serializable13writeExternalESt10shared_ptrINS_7ContentEE() == notSupported);
        Require(_ZN15AbstractStorage14MemfileContent12readExternalESt10shared_ptrINS_7ContentEE() == invalidArgument);
        Require(_ZN15AbstractStorage14MemfileContent13writeExternalESt10shared_ptrINS_7ContentEE() == invalidArgument);
        Require(_ZN15AbstractStorage4Item6RenameERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() == notSupported);
        Require(_ZNK15AbstractStorage4Item12GetThumbnailEPSt10shared_ptrINS_7ContentEE() == notSupported);
        Require(_ZNK15AbstractStorage4Item15CreateThumbnailEPvPSt10shared_ptrINS_7ContentEE() == notSupported);
    }
    return 0;
}
