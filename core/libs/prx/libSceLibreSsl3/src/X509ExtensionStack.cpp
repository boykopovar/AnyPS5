#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t ObjectDynamic = 0x01;
constexpr std::int32_t DynamicStrings = 0x04;
constexpr std::int32_t DynamicData = 0x08;

struct X509Extension {
    Asn1Object_nid_no_patch* object;
    std::int32_t critical;
    std::int32_t padding;
    Asn1String* value;
};

static_assert(sizeof(X509Extension) == 24);
static_assert(offsetof(X509Extension, critical) == 8);
static_assert(offsetof(X509Extension, value) == 16);

Asn1Object_nid_no_patch* DuplicateObject_nid_no_patch(const Asn1Object_nid_no_patch* source) {
    auto* copy = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!copy) return nullptr;
    *copy = *source;
    copy->flags = ObjectDynamic;
    if (source->flags & DynamicStrings) {
        copy->shortName = nullptr;
        copy->longName = nullptr;
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
    copy->data = nullptr;
    copy->length = 0;
    if (source->data) {
        if (source->length < 0) {
            ASN1_OBJECT_free_nid_postfix(copy);
            throw std::invalid_argument("X509_EXTENSION_dup: negative object data length");
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

Asn1String* DuplicateString_nid_no_patch(const Asn1String* source) {
    auto* copy = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!copy) return nullptr;
    copy->type = source->type;
    if (source->length < 0) {
        ASN1_OCTET_STRING_free_nid_postfix(copy);
        throw std::invalid_argument("X509_EXTENSION_dup: negative string length");
    }
    if (source->data && source->length != 0) {
        auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
        if (!data) {
            ASN1_OCTET_STRING_free_nid_postfix(copy);
            return nullptr;
        }
        std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
        copy->data = data;
        copy->length = source->length;
    }
    return copy;
}

X509Extension* DuplicateExtension_nid_no_patch(const X509Extension* source) {
    auto* copy = static_cast<X509Extension*>(calloc_nid_postfix(1, sizeof(X509Extension)));
    if (!copy) return nullptr;
    copy->critical = source->critical;
    if (source->object) {
        copy->object = DuplicateObject_nid_no_patch(source->object);
        if (!copy->object) {
            free_nid_postfix(copy);
            return nullptr;
        }
    }
    if (source->value) {
        copy->value = DuplicateString_nid_no_patch(source->value);
        if (!copy->value) {
            ASN1_OBJECT_free_nid_postfix(copy->object);
            free_nid_postfix(copy);
            return nullptr;
        }
    }
    return copy;
}

void FreeExtension_nid_no_patch(X509Extension* extension) {
    if (!extension) return;
    ASN1_OBJECT_free_nid_postfix(extension->object);
    ASN1_OCTET_STRING_free_nid_postfix(extension->value);
    free_nid_postfix(extension);
}

}

extern "C" {

void APS5_VABI X509_EXTENSION_free_nid_postfix(void* extension) {
    FreeExtension_nid_no_patch(static_cast<X509Extension*>(extension));
}

int APS5_VABI X509_add_ext_nid_postfix(void* x509, const void* extension, int location) {
    if (!x509 || !extension) return 0;
    auto* wrapper = static_cast<LibreSsl3ExtensionsX509*>(x509);
    auto* certificate = const_cast<LibreSsl3ExtensionsCertInfo*>(wrapper->certInfo);
    if (!certificate) return 0;
    auto* stack = reinterpret_cast<LibreSslStack*>(const_cast<LibreSsl3Stack*>(certificate->extensions));
    if (stack && stack->num < 0) throw std::invalid_argument(std::string(__func__) + ": negative extension count");
    auto* copy = DuplicateExtension_nid_no_patch(static_cast<const X509Extension*>(extension));
    if (!copy) return 0;
    bool created = false;
    if (!stack) {
        stack = sk_new_nid_postfix(nullptr);
        if (!stack) {
            FreeExtension_nid_no_patch(copy);
            return 0;
        }
        certificate->extensions = reinterpret_cast<const LibreSsl3Stack*>(stack);
        created = true;
    }
    if (location < 0 || location > stack->num) location = stack->num;
    if (!sk_push_nid_postfix(stack, copy)) {
        FreeExtension_nid_no_patch(copy);
        if (created) {
            sk_free_nid_postfix(stack);
            certificate->extensions = nullptr;
        }
        return 0;
    }
    if (location < stack->num - 1) {
        std::memmove(&stack->data[location + 1], &stack->data[location],
                     static_cast<std::size_t>(stack->num - 1 - location) * sizeof(char*));
        stack->data[location] = reinterpret_cast<char*>(copy);
    }
    return 1;
}

}
