#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <cstddef>
#include <cstdint>

namespace {

static_assert(sizeof(Asn1String) == sizeof(ASN1_STRING));
static_assert(offsetof(Asn1String, length) == offsetof(ASN1_STRING, length));
static_assert(offsetof(Asn1String, type) == offsetof(ASN1_STRING, type));
static_assert(offsetof(Asn1String, data) == offsetof(ASN1_STRING, data));
static_assert(offsetof(Asn1String, flags) == offsetof(ASN1_STRING, flags));
static_assert(sizeof(LibreSslAsn1Object) == sizeof(ASN1_OBJECT));
static_assert(offsetof(LibreSslAsn1Object, shortName) == offsetof(ASN1_OBJECT, sn));
static_assert(offsetof(LibreSslAsn1Object, longName) == offsetof(ASN1_OBJECT, ln));
static_assert(offsetof(LibreSslAsn1Object, nid) == offsetof(ASN1_OBJECT, nid));
static_assert(offsetof(LibreSslAsn1Object, length) == offsetof(ASN1_OBJECT, length));
static_assert(offsetof(LibreSslAsn1Object, data) == offsetof(ASN1_OBJECT, data));
static_assert(offsetof(LibreSslAsn1Object, flags) == offsetof(ASN1_OBJECT, flags));
static_assert(sizeof(LibreSslX509NameEntry) == sizeof(X509_NAME_ENTRY));
static_assert(offsetof(LibreSslX509NameEntry, object) == offsetof(X509_NAME_ENTRY, object));
static_assert(offsetof(LibreSslX509NameEntry, value) == offsetof(X509_NAME_ENTRY, value));
static_assert(offsetof(LibreSslX509NameEntry, set) == offsetof(X509_NAME_ENTRY, set));
static_assert(offsetof(LibreSslX509NameEntry, size) == offsetof(X509_NAME_ENTRY, size));
static_assert(sizeof(LibreSslX509Name) == sizeof(X509_NAME));
static_assert(offsetof(LibreSslX509Name, entries) == offsetof(X509_NAME, entries));
static_assert(offsetof(LibreSslX509Name, modified) == offsetof(X509_NAME, modified));
static_assert(offsetof(LibreSslX509Name, bytes) == offsetof(X509_NAME, bytes));
static_assert(offsetof(LibreSslX509Name, canonEnc) == offsetof(X509_NAME, canon_enc));
static_assert(offsetof(LibreSslX509Name, canonEnclen) == offsetof(X509_NAME, canon_enclen));
static_assert(sizeof(STACK_OF(X509_NAME_ENTRY)) == sizeof(_STACK));
static_assert(offsetof(STACK_OF(X509_NAME_ENTRY), stack.num) == offsetof(LibreSslStack, num));
static_assert(offsetof(STACK_OF(X509_NAME_ENTRY), stack.data) == offsetof(LibreSslStack, data));
static_assert(offsetof(STACK_OF(X509_NAME_ENTRY), stack.sorted) == offsetof(LibreSslStack, sorted));
static_assert(offsetof(STACK_OF(X509_NAME_ENTRY), stack.num_alloc) == offsetof(LibreSslStack, numAlloc));

}

extern "C" {

int APS5_VABI X509_NAME_print_ex_nid_postfix(LibreSsl::Bio* bio, const LibreSslX509Name* name, int indent, std::uint64_t flags) {
    if (!bio && flags == 0) return 0;
    X509_NAME empty{};
    const X509_NAME* real;
    if (name) real = reinterpret_cast<const X509_NAME*>(name);
    else real = flags == 0 ? nullptr : &empty;
    BIO* memory = BIO_new(BIO_s_mem());
    if (!memory) return -1;
    int result = X509_NAME_print_ex(memory, real, indent, static_cast<unsigned long>(flags));
    if (bio) {
        char* data = nullptr;
        const long length = BIO_get_mem_data(memory, &data);
        if (length > 0 && LibreSslBioWrite_nid_no_patch(bio, data, static_cast<int>(length)) != length)
            result = flags == 0 ? 0 : -1;
    }
    BIO_free(memory);
    return result;
}

}
