#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"

namespace {
constexpr int Undefined = -1;
constexpr std::int64_t MString = 0x40;
}

extern "C" Asn1String* APS5_VABI ASN1_TIME_new_nid_postfix() {
    auto* time = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!time) return nullptr;
    time->type = Undefined;
    time->flags = MString;
    return time;
}
