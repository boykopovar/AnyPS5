#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

static_assert(sizeof(LibreSslGeneralName) == 16);
static_assert(offsetof(LibreSslGeneralName, value) == 8);
static_assert(sizeof(Asn1String) == 24);
static_assert(sizeof(LibreSslAsn1Object) == 40);
static_assert(sizeof(LibreSslOcspResponse) == 16);
static_assert(offsetof(LibreSslOcspResponse, responseBytes) == 8);
static_assert(sizeof(LibreSslOcspResponseBytes) == 16);
static_assert(offsetof(LibreSslOcspResponseBytes, response) == 8);

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

template <typename T>
T Load(const T* source, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void StoreBytes(void* destination, const void* source, std::size_t size, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
void Store(T* destination, const T& value, const char* function) {
    StoreBytes(destination, &value, sizeof(T), function);
}

void Wipe(void* pointer, std::size_t size, const char* function) {
    static constexpr unsigned char zeros[4096]{};
    auto* bytes = static_cast<unsigned char*>(pointer);
    for (std::size_t done = 0; done < size;) {
        const std::size_t chunk = std::min(size - done, sizeof(zeros));
        StoreBytes(bytes + done, zeros, chunk, function);
        done += chunk;
    }
}

void FreeString(Asn1String* string, const char* function) {
    if (!string) return;
    const Asn1String value = Load(string, function);
    if (value.data && !(value.flags & Asn1StringNdef)) {
        if (value.length < 0) throw std::invalid_argument("ASN1_STRING_free: negative length");
        Wipe(value.data, static_cast<std::size_t>(value.length), function);
        free_nid_postfix(value.data);
    }
    free_nid_postfix(string);
}

void FreeObject(LibreSslAsn1Object* object, const char* function) {
    if (!object) return;
    LibreSslAsn1Object value = Load(object, function);
    if (value.flags & DynamicStrings) {
        free_nid_postfix(const_cast<char*>(value.shortName));
        free_nid_postfix(const_cast<char*>(value.longName));
        value.shortName = nullptr;
        value.longName = nullptr;
        Store(object, value, function);
    }
    if (value.flags & DynamicData) {
        if (value.data) {
            if (value.length < 0) throw std::invalid_argument("ASN1_OBJECT_free: negative length");
            Wipe(const_cast<unsigned char*>(value.data), static_cast<std::size_t>(value.length), function);
        }
        free_nid_postfix(const_cast<unsigned char*>(value.data));
        value.data = nullptr;
        value.length = 0;
        Store(object, value, function);
    }
    if (value.flags & DynamicObject) free_nid_postfix(object);
}

bool InvalidOwnedString(const Asn1String* string, const char* function) {
    if (!string) return false;
    const Asn1String value = Load(string, function);
    return value.data && !(value.flags & Asn1StringNdef) && value.length < 0;
}

void CheckResponseBytes(const LibreSslOcspResponseBytes* bytes, const char* function) {
    if (!bytes) return;
    const LibreSslOcspResponseBytes value = Load(bytes, function);
    if (InvalidOwnedString(value.response, function))
        throw std::invalid_argument("OCSP_RESPONSE_free: negative owned string length");
    if (!value.responseType) return;
    const LibreSslAsn1Object object = Load(value.responseType, function);
    if ((object.flags & DynamicData) && object.data && object.length < 0)
        throw std::invalid_argument("OCSP_RESPONSE_free: negative owned object length");
}

void ReleaseResponseBytes(LibreSslOcspResponseBytes* bytes, const char* function) {
    LibreSslOcspResponseBytes value = Load(bytes, function);
    FreeString(value.response, function);
    value.response = nullptr;
    Store(bytes, value, function);
    FreeObject(value.responseType, function);
    value.responseType = nullptr;
    Store(bytes, value, function);
    free_nid_postfix(bytes);
}

void FreeAny(Asn1Type* any, const char* function) {
    if (!any) return;
    const Asn1Type value = Load(any, function);
    if (value.value) {
        if (value.type == Asn1Object) FreeObject(static_cast<LibreSslAsn1Object*>(value.value), function);
        else if (value.type == Asn1Any) FreeAny(static_cast<Asn1Type*>(value.value), function);
        else if (value.type != Asn1Boolean && value.type != Asn1Null) FreeString(static_cast<Asn1String*>(value.value), function);
    }
    free_nid_postfix(any);
}

void FreeBuffer(Buffer* buffer, const char* function) {
    if (!buffer) return;
    const Buffer value = Load(buffer, function);
    if (value.data) {
        Wipe(value.data, value.capacity, function);
        free_nid_postfix(value.data);
    }
    free_nid_postfix(buffer);
}

void FreeX509Name(X509Name* name, const char* function) {
    if (!name) return;
    const X509Name value = Load(name, function);
    FreeBuffer(value.bytes, function);
    sk_pop_free_nid_postfix(value.entries, +[](void* pointer) APS5_VABI {
        auto* entry = static_cast<X509NameEntry*>(pointer);
        if (!entry) return;
        const X509NameEntry fields = Load(entry, "X509_NAME_ENTRY_free");
        FreeString(fields.value, "X509_NAME_ENTRY_free");
        FreeObject(fields.object, "X509_NAME_ENTRY_free");
        free_nid_postfix(entry);
    });
    free_nid_postfix(value.canonical);
    free_nid_postfix(name);
}

}

extern "C" {

void APS5_VABI GENERAL_NAME_free_nid_postfix(LibreSslGeneralName* name) {
    if (!name) return;
    const LibreSslGeneralName value = Load(name, __func__);
    switch (value.type) {
    case 0: {
        auto* other = static_cast<OtherName*>(value.value);
        if (other) {
            const OtherName fields = Load(other, __func__);
            FreeAny(fields.value, __func__);
            FreeObject(fields.typeId, __func__);
            free_nid_postfix(other);
        }
        break;
    }
    case 1:
    case 2:
    case 3:
    case 6:
    case 7:
        FreeString(static_cast<Asn1String*>(value.value), __func__);
        break;
    case 4:
        FreeX509Name(static_cast<X509Name*>(value.value), __func__);
        break;
    case 5: {
        auto* edi = static_cast<EdiPartyName*>(value.value);
        if (edi) {
            const EdiPartyName fields = Load(edi, __func__);
            FreeString(fields.partyName, __func__);
            FreeString(fields.nameAssigner, __func__);
            free_nid_postfix(edi);
        }
        break;
    }
    case 8:
        FreeObject(static_cast<LibreSslAsn1Object*>(value.value), __func__);
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

void LibreSslWipe_nid_no_patch(void* pointer, std::size_t size) {
    Wipe(pointer, size, __func__);
}

void LibreSslAsn1StringFree_nid_no_patch(Asn1String* string) {
    FreeString(string, __func__);
}

void LibreSslAsn1ObjectFree_nid_no_patch(LibreSslAsn1Object* object) {
    FreeObject(object, __func__);
}

void LibreSslAsn1TypeFree_nid_no_patch(void* type) {
    FreeAny(static_cast<Asn1Type*>(type), __func__);
}

void LibreSslX509NameFree_nid_no_patch(void* name) {
    FreeX509Name(static_cast<X509Name*>(name), __func__);
}

void LibreSslOcspResponseBytesFree_nid_no_patch(LibreSslOcspResponseBytes* bytes) {
    if (!bytes) return;
    CheckResponseBytes(bytes, __func__);
    ReleaseResponseBytes(bytes, __func__);
}

void APS5_VABI OCSP_RESPONSE_free_nid_postfix(LibreSslOcspResponse* response) {
    if (!response) return;
    LibreSslOcspResponse value = Load(response, __func__);
    auto* bytes = static_cast<LibreSslOcspResponseBytes*>(value.responseBytes);
    if (InvalidOwnedString(value.responseStatus, __func__))
        throw std::invalid_argument("OCSP_RESPONSE_free: negative owned string length");
    CheckResponseBytes(bytes, __func__);
    if (bytes) {
        ReleaseResponseBytes(bytes, __func__);
        value.responseBytes = nullptr;
        Store(response, value, __func__);
    }
    FreeString(value.responseStatus, __func__);
    value.responseStatus = nullptr;
    Store(response, value, __func__);
    free_nid_postfix(response);
}

}
