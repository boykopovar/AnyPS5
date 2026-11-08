#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int APS5_VABI X509_get_signature_nid_nid_postfix(const LibreSsl3X509* certificate) {
    if (!certificate || !certificate->sigAlg) return 0;
    const auto* algorithm = static_cast<const LibreSsl3X509Algor*>(certificate->sigAlg);
    const auto* object = static_cast<const Asn1Object_nid_no_patch*>(algorithm->algorithm);
    if (!object) return 0;
    if (object->nid == 0 && object->data && object->length > 0)
        NotImplemented_nid_no_patch("X509_get_signature_nid: OID lookup");
    return object->nid;
}
