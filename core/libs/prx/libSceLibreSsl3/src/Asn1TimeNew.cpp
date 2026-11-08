#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"

namespace {
constexpr int UtcTime = 23;
}

extern "C" Asn1String* APS5_VABI ASN1_TIME_new_nid_postfix() {
    auto* time = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!time) return nullptr;
    time->type = UtcTime;
    return time;
}
