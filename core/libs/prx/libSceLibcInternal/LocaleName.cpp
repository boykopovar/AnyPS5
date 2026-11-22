#include "prx/libSceLibcInternal/include/LocaleName.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <string>

extern "C" {

void APS5_VABI _ZSt15_sceLibcLocinfoPKc_nid_postfix(const char* localeName) {
    std::string name;
    if (localeName == nullptr || CopyGuestString_nid_no_patch(localeName, 2, name) != 0 || (!name.empty() && name != "C")) {
        _ZSt15_Xruntime_errorPKc_nid_postfix("bad locale name");
    }
}

}
