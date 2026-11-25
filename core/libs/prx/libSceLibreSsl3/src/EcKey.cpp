#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>

extern "C" {

void APS5_VABI EC_KEY_set_asn1_flag_nid_postfix(EcKey* key, int flag) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    EcGroup* group = nullptr;
    bool copied = GuestCopyIn_nid_no_patch(&group, &key->group, sizeof(group));
    if (!copied) throw std::invalid_argument("EC_KEY_set_asn1_flag: unreadable key");
    if (!group) return;
    const std::int32_t value = flag;
    copied = GuestCopyOut_nid_no_patch(&group->asn1Flag, &value, sizeof(value));
    if (!copied) throw std::invalid_argument("EC_KEY_set_asn1_flag: unwritable group");
}

}
