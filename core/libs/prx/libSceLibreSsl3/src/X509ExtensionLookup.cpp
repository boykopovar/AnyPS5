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

}

extern "C" {

int APS5_VABI X509_get_ext_by_NID_nid_postfix(const LibreSsl3ExtensionsX509* certificate, int nid, int lastPosition) {
    if (!certificate) return -1;
    const int count = X509_get_ext_count_nid_postfix(certificate);
    if (lastPosition < 0) lastPosition = -1;
    for (int index = lastPosition + 1; index < count; ++index)
    {
        auto* extension = static_cast<const Extension*>(X509_get_ext_nid_postfix(certificate, index));
        if (extension && extension->object && extension->object->nid == nid) return index;
    }
    return -1;
}

}
