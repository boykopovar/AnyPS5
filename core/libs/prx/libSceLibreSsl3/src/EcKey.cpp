#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI EC_KEY_set_asn1_flag_nid_postfix(EcKey* key, int flag) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    if (key->group) key->group->asn1Flag = flag;
}

}
