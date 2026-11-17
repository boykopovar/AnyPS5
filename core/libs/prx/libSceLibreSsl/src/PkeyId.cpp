#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <stdexcept>

namespace {
struct PkeyHeader {
    int type;
};
}

extern "C" int APS5_VABI EVP_PKEY_id_nid_postfix(const void* value) {
    if (!value) throw std::invalid_argument("EVP_PKEY_id: null key");
    return static_cast<const PkeyHeader*>(value)->type;
}
