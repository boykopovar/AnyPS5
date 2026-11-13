#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <cstddef>
#include <cstdint>
#include <vector>

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

int CharacterWidth(int type) {
    if (type == V_ASN1_BMPSTRING) return 2;
    if (type == V_ASN1_UNIVERSALSTRING) return 4;
    return 1;
}

bool ReachesCharacterDecoder(const X509_NAME_ENTRY* entry, unsigned long flags) {
    if (flags & (ASN1_STRFLGS_DUMP_ALL | ASN1_STRFLGS_IGNORE_TYPE)) return false;
    return !(OBJ_obj2nid(entry->object) == NID_undef && (flags & XN_FLAG_DUMP_UNKNOWN_FIELDS));
}

bool PartialCharacter(const X509_NAME_ENTRY* entry, unsigned long flags) {
    const ASN1_STRING* value = entry->value;
    return value && value->length % CharacterWidth(value->type) != 0 && ReachesCharacterDecoder(entry, flags);
}

class RejectedPartialCharacters {
public:
    RejectedPartialCharacters(const X509_NAME* name, unsigned long flags) {
        const int count = sk_X509_NAME_ENTRY_num(name->entries);
        bool partial = false;
        for (int index = 0; index < count && !partial; ++index)
            partial = PartialCharacter(sk_X509_NAME_ENTRY_value(name->entries, index), flags);
        if (!partial) return;
        entries_.reserve(static_cast<std::size_t>(count));
        values_.reserve(static_cast<std::size_t>(count));
        stack_ = sk_X509_NAME_ENTRY_new_null();
        if (!stack_) {
            failed_ = true;
            return;
        }
        for (int index = 0; index < count; ++index) {
            entries_.push_back(*sk_X509_NAME_ENTRY_value(name->entries, index));
            X509_NAME_ENTRY& entry = entries_.back();
            if (PartialCharacter(&entry, flags)) {
                values_.push_back(*entry.value);
                values_.back().data = invalid_;
                values_.back().length = CharacterWidth(entry.value->type);
                entry.value = &values_.back();
            }
            if (!sk_X509_NAME_ENTRY_push(stack_, &entry)) {
                failed_ = true;
                return;
            }
        }
        name_ = *name;
        name_.entries = stack_;
        replaced_ = &name_;
    }

    ~RejectedPartialCharacters() {
        if (stack_) sk_X509_NAME_ENTRY_free(stack_);
    }

    RejectedPartialCharacters(const RejectedPartialCharacters&) = delete;
    RejectedPartialCharacters& operator=(const RejectedPartialCharacters&) = delete;

    const X509_NAME* Replace(const X509_NAME* name) const { return replaced_ ? replaced_ : name; }
    bool Failed() const { return failed_; }

private:
    unsigned char invalid_[4]{0xD8, 0x00, 0xFF, 0xFF};
    std::vector<X509_NAME_ENTRY> entries_;
    std::vector<ASN1_STRING> values_;
    STACK_OF(X509_NAME_ENTRY)* stack_ = nullptr;
    X509_NAME name_{};
    const X509_NAME* replaced_ = nullptr;
    bool failed_ = false;
};

}

extern "C" {

int APS5_VABI X509_NAME_print_ex_nid_postfix(LibreSsl::Bio* bio, const LibreSslX509Name* name, int indent, std::uint64_t flags) {
    if (!bio && flags == 0) return 0;
    X509_NAME empty{};
    const X509_NAME* real;
    if (name) real = reinterpret_cast<const X509_NAME*>(name);
    else real = flags == 0 ? nullptr : &empty;
    const RejectedPartialCharacters rejected(real && flags != XN_FLAG_COMPAT ? real : &empty, static_cast<unsigned long>(flags));
    if (rejected.Failed()) return -1;
    BIO* memory = BIO_new(BIO_s_mem());
    if (!memory) return -1;
    int result = X509_NAME_print_ex(memory, rejected.Replace(real), indent, static_cast<unsigned long>(flags));
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
