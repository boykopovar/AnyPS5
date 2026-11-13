#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

static_assert(sizeof(AuthorityKeyId) == 24);
static_assert(offsetof(AuthorityKeyId, issuer) == 8);

extern "C" void APS5_VABI AUTHORITY_KEYID_free_nid_postfix(void* pointer) {
    if (!pointer) return;
    auto* id = static_cast<AuthorityKeyId*>(pointer);
    ASN1_OCTET_STRING_free_nid_postfix(id->keyId);
    GENERAL_NAMES_free_nid_postfix(id->issuer);
    ASN1_INTEGER_free_nid_postfix(id->serial);
    free_nid_postfix(id);
}
