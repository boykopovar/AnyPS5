#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include <cstddef>

namespace {

struct Extension {
    const Asn1Object_nid_no_patch* object;
    int critical;
    int padding;
    const void* value;
};

static_assert(sizeof(Extension) == 24);
static_assert(offsetof(Extension, object) == 0);

}

extern "C" {

int APS5_VABI X509_get_ext_by_NID_nid_postfix(const LibreSsl3ExtensionsX509* certificate, int nid, int lastPosition) {
    if (!certificate) return -1;
    if (nid < 0 || nid >= LibreSslObjectCount) return -2;
    const LibreSslAsn1Object& object = LibreSslObjects_nid_no_patch[nid];
    if (nid != 0 && object.nid == 0) return -2;
    const int count = X509_get_ext_count_nid_postfix(certificate);
    int index = lastPosition < 0 ? 0 : lastPosition + 1;
    for (; index < count; ++index)
    {
        const auto* extension = static_cast<const Extension*>(X509_get_ext_nid_postfix(certificate, index));
        if (extension && extension->object && OBJ_cmp_nid_postfix(extension->object, &object) == 0) return index;
    }
    return -1;
}

}
