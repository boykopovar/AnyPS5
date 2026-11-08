#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <stdexcept>
#include <string>

namespace {

struct Extension {
    const Asn1Object_nid_no_patch* object;
    int critical;
    int padding;
    const void* value;
};

static_assert(sizeof(Extension) == 24);
static_assert(offsetof(Extension, object) == 0);

struct KnownOid {
    int nid;
    int length;
    unsigned char data[9];
};

constexpr KnownOid KnownOids[] = {
    {71, 9, {0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x01, 0x01}},
    {82, 3, {0x55, 0x1d, 0x0e, 0, 0, 0, 0, 0, 0}},
    {83, 3, {0x55, 0x1d, 0x0f, 0, 0, 0, 0, 0, 0}},
    {84, 3, {0x55, 0x1d, 0x10, 0, 0, 0, 0, 0, 0}},
    {85, 3, {0x55, 0x1d, 0x11, 0, 0, 0, 0, 0, 0}},
    {86, 3, {0x55, 0x1d, 0x12, 0, 0, 0, 0, 0, 0}},
    {87, 3, {0x55, 0x1d, 0x13, 0, 0, 0, 0, 0, 0}},
    {88, 3, {0x55, 0x1d, 0x1e, 0, 0, 0, 0, 0, 0}},
    {89, 3, {0x55, 0x1d, 0x20, 0, 0, 0, 0, 0, 0}},
    {90, 3, {0x55, 0x1d, 0x23, 0, 0, 0, 0, 0, 0}},
    {91, 3, {0x55, 0x1d, 0x24, 0, 0, 0, 0, 0, 0}},
    {92, 3, {0x55, 0x1d, 0x25, 0, 0, 0, 0, 0, 0}},
    {93, 3, {0x55, 0x1d, 0x1f, 0, 0, 0, 0, 0, 0}},
    {94, 3, {0x55, 0x1d, 0x36, 0, 0, 0, 0, 0, 0}},
    {95, 3, {0x55, 0x1d, 0x2e, 0, 0, 0, 0, 0, 0}},
    {96, 8, {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01, 0}},
    {97, 8, {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x0b, 0}},
};

const KnownOid* Lookup(int nid) {
    for (const auto& known : KnownOids)
        if (known.nid == nid) return &known;
    return nullptr;
}

bool Matches(const Extension& extension, const KnownOid& known) {
    if (!extension.object) return false;
    if (extension.object->nid == known.nid) return true;
    if (extension.object->nid != 0 || extension.object->length != known.length || !extension.object->data) return false;
    for (int index = 0; index < known.length; ++index)
        if (extension.object->data[index] != known.data[index]) return false;
    return true;
}

}

extern "C" {

int APS5_VABI X509_get_ext_by_NID_nid_postfix(const LibreSsl3ExtensionsX509* certificate, int nid, int lastPosition) {
    if (!certificate) return -1;
    const KnownOid* known = Lookup(nid);
    if (!known) return -2;
    const int count = X509_get_ext_count_nid_postfix(certificate);
    if (lastPosition < 0) lastPosition = -1;
    for (int index = lastPosition + 1; index < count; ++index)
    {
        auto* extension = static_cast<const Extension*>(X509_get_ext_nid_postfix(certificate, index));
        if (extension && Matches(*extension, *known)) return index;
    }
    return -1;
}

}
