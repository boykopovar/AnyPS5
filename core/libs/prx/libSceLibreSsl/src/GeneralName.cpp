#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

static_assert(sizeof(LibreSslGeneralName) == 16);
static_assert(offsetof(LibreSslGeneralName, value) == 8);
static_assert(sizeof(Asn1String) == 24);
static_assert(sizeof(LibreSslAsn1Object) == 40);

namespace {

constexpr std::int64_t Asn1StringNdef = 0x10;
constexpr int Asn1Boolean = 1;
constexpr int Asn1Null = 5;
constexpr int Asn1Object = 6;
constexpr int Asn1Any = -4;
constexpr int DynamicObject = 0x01;
constexpr int DynamicStrings = 0x04;
constexpr int DynamicData = 0x08;

struct Asn1Type {
    int type;
    void* value;
};

struct OtherName {
    LibreSslAsn1Object* typeId;
    Asn1Type* value;
};

struct EdiPartyName {
    Asn1String* nameAssigner;
    Asn1String* partyName;
};

struct Buffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

struct X509NameEntry {
    LibreSslAsn1Object* object;
    Asn1String* value;
    int set;
    int size;
};

struct X509Name {
    LibreSslStack* entries;
    int modified;
    Buffer* bytes;
    unsigned char* canonical;
    int canonicalLength;
};

static_assert(sizeof(Asn1Type) == 16 && offsetof(Asn1Type, value) == 8);
static_assert(sizeof(OtherName) == 16 && sizeof(EdiPartyName) == 16);
static_assert(sizeof(Buffer) == 24 && offsetof(Buffer, capacity) == 16);
static_assert(sizeof(X509NameEntry) == 24 && offsetof(X509NameEntry, set) == 16);
static_assert(sizeof(X509Name) == 40 && offsetof(X509Name, bytes) == 16);
static_assert(offsetof(X509Name, canonical) == 24);

void Wipe(void* pointer, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(pointer);
    for (std::size_t i = 0; i < size; ++i) bytes[i] = 0;
}

void FreeString(Asn1String* string) {
    if (!string) return;
    if (string->data && !(string->flags & Asn1StringNdef)) {
        if (string->length < 0) throw std::invalid_argument("ASN1_STRING_free: negative length");
        Wipe(string->data, static_cast<std::size_t>(string->length));
        free_nid_postfix(string->data);
    }
    free_nid_postfix(string);
}

void FreeObject(LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & DynamicStrings) {
        free_nid_postfix(const_cast<char*>(object->shortName));
        free_nid_postfix(const_cast<char*>(object->longName));
        object->shortName = nullptr;
        object->longName = nullptr;
    }
    if (object->flags & DynamicData) {
        if (object->data) {
            if (object->length < 0) throw std::invalid_argument("ASN1_OBJECT_free: negative length");
            Wipe(const_cast<unsigned char*>(object->data), static_cast<std::size_t>(object->length));
        }
        free_nid_postfix(const_cast<unsigned char*>(object->data));
        object->data = nullptr;
        object->length = 0;
    }
    if (object->flags & DynamicObject) free_nid_postfix(object);
}

void FreeAny(Asn1Type* any) {
    if (!any) return;
    if (any->value) {
        if (any->type == Asn1Object) FreeObject(static_cast<LibreSslAsn1Object*>(any->value));
        else if (any->type == Asn1Any) FreeAny(static_cast<Asn1Type*>(any->value));
        else if (any->type != Asn1Boolean && any->type != Asn1Null) FreeString(static_cast<Asn1String*>(any->value));
    }
    free_nid_postfix(any);
}

void FreeBuffer(Buffer* buffer) {
    if (!buffer) return;
    if (buffer->data) {
        Wipe(buffer->data, buffer->capacity);
        free_nid_postfix(buffer->data);
    }
    free_nid_postfix(buffer);
}

void FreeX509Name(X509Name* name) {
    if (!name) return;
    FreeBuffer(name->bytes);
    sk_pop_free_nid_postfix(name->entries, +[](void* pointer) APS5_VABI {
        auto* entry = static_cast<X509NameEntry*>(pointer);
        if (!entry) return;

        FreeString(entry->value);
        FreeObject(entry->object);
        free_nid_postfix(entry);
    });
    free_nid_postfix(name->canonical);
    free_nid_postfix(name);
}

}

extern "C" {

void APS5_VABI GENERAL_NAME_free_nid_postfix(LibreSslGeneralName* name) {
    if (!name) return;
    switch (name->type) {
    case 0: {
        auto* other = static_cast<OtherName*>(name->value);
        if (other) {
            FreeAny(other->value);
            FreeObject(other->typeId);
            free_nid_postfix(other);
        }
        break;
    }
    case 1:
    case 2:
    case 3:
    case 6:
    case 7:
        FreeString(static_cast<Asn1String*>(name->value));
        break;
    case 4:
        FreeX509Name(static_cast<X509Name*>(name->value));
        break;
    case 5: {
        auto* edi = static_cast<EdiPartyName*>(name->value);
        if (edi) {
            FreeString(edi->partyName);
            FreeString(edi->nameAssigner);
            free_nid_postfix(edi);
        }
        break;
    }
    case 8:
        FreeObject(static_cast<LibreSslAsn1Object*>(name->value));
        break;
    default:
        break;
    }
    free_nid_postfix(name);
}

void APS5_VABI GENERAL_NAMES_free_nid_postfix(LibreSslStack* names) {
    sk_pop_free_nid_postfix(names, +[](void* pointer) APS5_VABI {
        GENERAL_NAME_free_nid_postfix(static_cast<LibreSslGeneralName*>(pointer));
    });
}

}
