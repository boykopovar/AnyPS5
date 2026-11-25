#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

static_assert(sizeof(AuthorityKeyId) == 24 && offsetof(AuthorityKeyId, serial) == 16);

AuthorityKeyId ReadAuthorityKeyId(const void* source) {
    AuthorityKeyId value{};
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    if (!copied) throw std::invalid_argument(std::string(__func__) + ": unreadable authority key identifier");
    return value;
}

}

extern "C" {

void APS5_VABI AUTHORITY_KEYID_free_nid_postfix(void* authorityKeyId) {
    if (!authorityKeyId) return;
    const AuthorityKeyId value = ReadAuthorityKeyId(authorityKeyId);
    ASN1_OCTET_STRING_free_nid_postfix(value.keyId);
    GENERAL_NAMES_free_nid_postfix(value.issuer);
    ASN1_INTEGER_free_nid_postfix(value.serial);
    free_nid_postfix(authorityKeyId);
}

}
