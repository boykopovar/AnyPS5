#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int32_t ObjectDynamic = 0x01;
constexpr std::int32_t DynamicStrings = 0x04;
constexpr std::int32_t DynamicData = 0x08;

struct Buffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

static_assert(sizeof(LibreSslStack) == 32);
static_assert(offsetof(LibreSslStack, data) == 8);
static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24);
static_assert(sizeof(Buffer) == 24);
static_assert(offsetof(Buffer, data) == 8);

Asn1String* DuplicateString(const Asn1String* source) {
    auto* copy = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!copy) return nullptr;
    copy->type = source->type;
    if (source->length < 0) {
        free_nid_postfix(copy);
        throw std::invalid_argument("X509_NAME_dup: negative string length");
    }
    if (source->data && source->length != 0) {
        auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
        if (!data) {
            free_nid_postfix(copy);
            return nullptr;
        }
        std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
        copy->data = data;
        copy->length = source->length;
    }
    return copy;
}

Asn1Object_nid_no_patch* DuplicateObject(const Asn1Object_nid_no_patch* source) {
    auto* copy = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!copy) return nullptr;
    *copy = *source;
    copy->flags = ObjectDynamic;
    copy->shortName = nullptr;
    copy->longName = nullptr;
    copy->data = nullptr;
    copy->length = 0;
    if (source->flags & DynamicStrings) {
        if (source->shortName) {
            const std::size_t length = std::strlen(source->shortName) + 1;
            auto* text = static_cast<char*>(malloc_nid_postfix(length));
            if (!text) {
                ASN1_OBJECT_free_nid_postfix(copy);
                return nullptr;
            }
            std::memcpy(text, source->shortName, length);
            copy->shortName = text;
        }
        if (source->longName) {
            const std::size_t length = std::strlen(source->longName) + 1;
            auto* text = static_cast<char*>(malloc_nid_postfix(length));
            if (!text) {
                ASN1_OBJECT_free_nid_postfix(copy);
                return nullptr;
            }
            std::memcpy(text, source->longName, length);
            copy->longName = text;
        }
        copy->flags |= DynamicStrings;
    }
    if (source->data) {
        if (source->length < 0) {
            ASN1_OBJECT_free_nid_postfix(copy);
            throw std::invalid_argument("X509_NAME_dup: negative object data length");
        }
        if (source->length != 0) {
            auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
            if (!data) {
                ASN1_OBJECT_free_nid_postfix(copy);
                return nullptr;
            }
            std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
            copy->data = data;
            copy->length = source->length;
        }
        copy->flags |= DynamicData;
    }
    return copy;
}

void FreeEntry(LibreSslX509NameEntry* entry) {
    if (!entry) return;
    ASN1_OCTET_STRING_free_nid_postfix(static_cast<Asn1String*>(entry->value));
    ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(entry->object));
    free_nid_postfix(entry);
}

LibreSslX509NameEntry* DuplicateEntry(const LibreSslX509NameEntry* source) {
    auto* copy = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!copy) return nullptr;
    copy->set = source->set;
    copy->size = source->size;
    if (source->object) {
        copy->object = DuplicateObject(static_cast<const Asn1Object_nid_no_patch*>(source->object));
        if (!copy->object) {
            free_nid_postfix(copy);
            return nullptr;
        }
    }
    if (source->value) {
        copy->value = DuplicateString(static_cast<const Asn1String*>(source->value));
        if (!copy->value) {
            FreeEntry(copy);
            return nullptr;
        }
    }
    return copy;
}

LibreSslStack* DuplicateEntries(const LibreSslStack* source) {
    auto* copy = static_cast<LibreSslStack*>(calloc_nid_postfix(1, sizeof(LibreSslStack)));
    if (!copy) return nullptr;
    copy->sorted = source->sorted;
    copy->comp = source->comp;
    if (source->num < 0) {
        free_nid_postfix(copy);
        throw std::invalid_argument("X509_NAME_dup: negative entry count");
    }
    copy->num = source->num;
    copy->numAlloc = source->num;
    if (source->num == 0) return copy;
    if (!source->data) {
        free_nid_postfix(copy);
        throw std::invalid_argument("X509_NAME_dup: null entry array");
    }
    copy->data = static_cast<char**>(calloc_nid_postfix(static_cast<std::size_t>(source->num), sizeof(char*)));
    if (!copy->data) {
        free_nid_postfix(copy);
        return nullptr;
    }
    for (int index = 0; index < source->num; ++index) {
        if (!source->data[index]) continue;
        auto* entry = DuplicateEntry(reinterpret_cast<const LibreSslX509NameEntry*>(source->data[index]));
        if (!entry) {
            for (int copied = 0; copied < index; ++copied)
                FreeEntry(reinterpret_cast<LibreSslX509NameEntry*>(copy->data[copied]));
            free_nid_postfix(copy->data);
            free_nid_postfix(copy);
            return nullptr;
        }
        copy->data[index] = reinterpret_cast<char*>(entry);
    }
    return copy;
}

Buffer* DuplicateBuffer(const Buffer* source) {
    auto* copy = static_cast<Buffer*>(calloc_nid_postfix(1, sizeof(Buffer)));
    if (!copy) return nullptr;
    if (!source->data) return copy;
    if (source->capacity < source->length || source->capacity == 0) {
        free_nid_postfix(copy);
        throw std::invalid_argument("X509_NAME_dup: invalid encoded name buffer");
    }
    auto* data = static_cast<char*>(calloc_nid_postfix(source->capacity, 1));
    if (!data) {
        free_nid_postfix(copy);
        return nullptr;
    }
    std::memcpy(data, source->data, source->length);
    copy->data = data;
    copy->length = source->length;
    copy->capacity = source->capacity;
    return copy;
}

LibreSslX509Name* DuplicateName(const LibreSslX509Name* source) {
    auto* copy = static_cast<LibreSslX509Name*>(calloc_nid_postfix(1, sizeof(LibreSslX509Name)));
    if (!copy) return nullptr;
    copy->modified = source->modified;
    if (source->canonEnc) {
        if (source->canonEnclen < 0) {
            X509_NAME_free_nid_no_patch(copy);
            throw std::invalid_argument("X509_NAME_dup: negative canonical encoding length");
        }
        if (source->canonEnclen != 0) {
            auto* canonical = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->canonEnclen)));
            if (!canonical) {
                X509_NAME_free_nid_no_patch(copy);
                return nullptr;
            }
            std::memcpy(canonical, source->canonEnc, static_cast<std::size_t>(source->canonEnclen));
            copy->canonEnc = canonical;
            copy->canonEnclen = source->canonEnclen;
        }
    }
    if (source->entries) {
        copy->entries = DuplicateEntries(source->entries);
        if (!copy->entries) {
            X509_NAME_free_nid_no_patch(copy);
            return nullptr;
        }
    }
    if (source->bytes) {
        copy->bytes = DuplicateBuffer(static_cast<const Buffer*>(source->bytes));
        if (!copy->bytes) {
            X509_NAME_free_nid_no_patch(copy);
            return nullptr;
        }
    }
    return copy;
}

}

extern "C" {

LibreSslX509Name* X509_NAME_dup_nid_no_patch(const LibreSslX509Name* name) {
    if (!name) return nullptr;
    return DuplicateName(name);
}

void X509_NAME_free_nid_no_patch(LibreSslX509Name* name) {
    LibreSslX509NameFree_nid_no_patch(name);
}

int APS5_VABI X509_set_issuer_name_nid_postfix(LibreSsl3Certificate* certificate, LibreSslX509Name* name) {
    if (!certificate || !certificate->certInfo || !name) return 0;
    auto* info = const_cast<LibreSsl3CertificateInfo*>(certificate->certInfo);
    if (info->issuer == name) return 1;
    auto* copy = DuplicateName(name);
    if (!copy) return 0;
    X509_NAME_free_nid_no_patch(static_cast<LibreSslX509Name*>(info->issuer));
    info->issuer = copy;
    return 1;
}

}
