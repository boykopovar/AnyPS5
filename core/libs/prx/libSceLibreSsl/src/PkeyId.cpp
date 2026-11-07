#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {
struct PkeyHeader {
    int type;
};
}

extern "C" int APS5_VABI EVP_PKEY_id_nid_postfix(const void* value) {
    if (!value) NotImplemented_nid_no_patch(__func__);
    return static_cast<const PkeyHeader*>(value)->type;
}
