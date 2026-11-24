#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int X509V3Library = 34;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailureReason = 65;
constexpr int ExtensionNotFoundReason = 102;
constexpr int UnsupportedOptionReason = 117;
constexpr int UnknownExtensionReason = 129;
constexpr int ErrorCreatingExtensionReason = 144;
constexpr int ExtensionExistsReason = 145;
constexpr int UnknownObjectPosition = -2;

constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicStrings = 0x04;
constexpr int ObjectDynamicData = 0x08;

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr int NegativeFlag = 0x100;

constexpr std::uint64_t OperationMask = 0x0f;
constexpr std::uint64_t DefaultOperation = 0;
constexpr std::uint64_t AppendOperation = 1;
constexpr std::uint64_t ReplaceOperation = 2;
constexpr std::uint64_t ReplaceExistingOperation = 3;
constexpr std::uint64_t KeepExistingOperation = 4;
constexpr std::uint64_t DeleteOperation = 5;
constexpr std::uint64_t SilentFlag = 0x10;

constexpr unsigned char IntegerTag = 2;
constexpr unsigned char BitStringTag = 3;
constexpr unsigned char OctetStringTag = 4;
constexpr unsigned char ObjectTag = 6;
constexpr unsigned char EnumeratedTag = 10;
constexpr unsigned char Ia5StringTag = 22;
constexpr unsigned char GeneralizedTimeTag = 24;
constexpr std::int32_t OctetStringType = 4;

enum class Encoder {
    Unsupported,
    BitString,
    OctetString,
    Integer,
    Enumerated,
    Ia5String,
    GeneralizedTime,
    Object,
    OcspNonce,
};

struct Method {
    int nid;
    Encoder encoder;
};

constexpr Method Methods[] = {
    {71, Encoder::BitString},
    {72, Encoder::Ia5String},
    {73, Encoder::Ia5String},
    {74, Encoder::Ia5String},
    {75, Encoder::Ia5String},
    {76, Encoder::Ia5String},
    {77, Encoder::Ia5String},
    {78, Encoder::Ia5String},
    {82, Encoder::OctetString},
    {83, Encoder::BitString},
    {84, Encoder::Unsupported},
    {85, Encoder::Unsupported},
    {86, Encoder::Unsupported},
    {87, Encoder::Unsupported},
    {88, Encoder::Integer},
    {89, Encoder::Unsupported},
    {90, Encoder::Unsupported},
    {103, Encoder::Unsupported},
    {126, Encoder::Unsupported},
    {140, Encoder::Integer},
    {141, Encoder::Enumerated},
    {142, Encoder::GeneralizedTime},
    {143, Encoder::Unsupported},
    {177, Encoder::Unsupported},
    {290, Encoder::Unsupported},
    {291, Encoder::Unsupported},
    {366, Encoder::OcspNonce},
    {367, Encoder::Unsupported},
    {368, Encoder::Unsupported},
    {369, Encoder::Unsupported},
    {370, Encoder::GeneralizedTime},
    {371, Encoder::Unsupported},
    {398, Encoder::Unsupported},
    {401, Encoder::Unsupported},
    {430, Encoder::Object},
    {663, Encoder::Unsupported},
    {666, Encoder::Unsupported},
    {747, Encoder::Unsupported},
    {748, Encoder::Integer},
    {770, Encoder::Unsupported},
    {771, Encoder::Unsupported},
    {857, Encoder::Unsupported},
};

static_assert(sizeof(Methods) / sizeof(Methods[0]) == 42);

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(X509V3Library, AnyFunction, reason);
}

const Method* FindMethod(int nid) {
    for (const Method& method : Methods) {
        if (method.nid == nid) return &method;
    }
    return nullptr;
}

const unsigned char* CheckedData(const void* data, int length) {
    if (length < 0) throw std::invalid_argument("X509_add1_ext_i2d: negative value length");
    if (length > 0 && !data) throw std::invalid_argument("X509_add1_ext_i2d: null value data");
    return static_cast<const unsigned char*>(data);
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)];
    std::size_t count = 0;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8)
        bytes[count++] = static_cast<unsigned char>(remaining & 0xff);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) output.push_back(bytes[--count]);
}

std::vector<unsigned char> Element(unsigned char tag, const std::vector<unsigned char>& content) {
    std::vector<unsigned char> output;
    output.reserve(content.size() + 1 + sizeof(std::size_t));
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
    return output;
}

std::vector<unsigned char> StringContent(const Asn1String* string) {
    if (!string) throw std::invalid_argument("X509_add1_ext_i2d: null string value");
    const unsigned char* data = CheckedData(string->data, string->length);
    const std::size_t length = static_cast<std::size_t>(string->length);
    if (length == 0) return {};
    return std::vector<unsigned char>(data, data + length);
}

std::vector<unsigned char> BitStringContent(const Asn1String* string) {
    if (!string) throw std::invalid_argument("X509_add1_ext_i2d: null bit string value");
    const unsigned char* data = CheckedData(string->data, string->length);
    std::size_t length = static_cast<std::size_t>(string->length);
    int bits = 0;
    if (length > 0) {
        if ((string->flags & BitsLeftFlag) != 0) {
            bits = static_cast<int>(string->flags) & 0x07;
        } else {
            while (length > 0 && data[length - 1] == 0) --length;
            if (length > 0) {
                const unsigned char last = data[length - 1];
                if (last & 0x01) bits = 0;
                else if (last & 0x02) bits = 1;
                else if (last & 0x04) bits = 2;
                else if (last & 0x08) bits = 3;
                else if (last & 0x10) bits = 4;
                else if (last & 0x20) bits = 5;
                else if (last & 0x40) bits = 6;
                else bits = 7;
            }
        }
    }
    std::vector<unsigned char> content(length + 1);
    content[0] = static_cast<unsigned char>(bits);
    if (length != 0) {
        std::memcpy(content.data() + 1, data, length);
        content[length] &= static_cast<unsigned char>(0xffu << bits);
    }
    return content;
}

std::vector<unsigned char> IntegerContent(const Asn1String* string) {
    if (!string) throw std::invalid_argument("X509_add1_ext_i2d: null integer value");
    const unsigned char* data = CheckedData(string->data, string->length);
    const std::size_t length = static_cast<std::size_t>(string->length);
    const bool negative = (string->type & NegativeFlag) != 0;
    std::vector<unsigned char> content;
    if (length == 0) {
        content.push_back(0);
        return content;
    }
    bool pad = false;
    unsigned char padByte = 0;
    if (!negative) {
        if (data[0] > 127) {
            pad = true;
            padByte = 0;
        }
    } else if (data[0] > 128) {
        pad = true;
        padByte = 0xff;
    } else if (data[0] == 128) {
        for (std::size_t index = 1; index < length; ++index) {
            if (data[index] != 0) {
                pad = true;
                padByte = 0xff;
                break;
            }
        }
    }
    if (!negative) {
        if (pad) content.push_back(padByte);
        content.insert(content.end(), data, data + length);
        return content;
    }
    content.assign(length + (pad ? 1 : 0), 0);
    if (pad) content[0] = padByte;
    std::size_t position = content.size();
    std::size_t index = length;
    while (index > 0 && data[index - 1] == 0) {
        content[--position] = 0;
        --index;
    }
    if (index == 0) {
        content[0] = 1;
        content.push_back(0);
        return content;
    }
    content[--position] = static_cast<unsigned char>((data[index - 1] ^ 0xff) + 1);
    --index;
    while (index > 0) {
        content[--position] = static_cast<unsigned char>(data[index - 1] ^ 0xff);
        --index;
    }
    return content;
}

std::vector<unsigned char> ObjectContent(const LibreSslAsn1Object* object) {
    if (!object) throw std::invalid_argument("X509_add1_ext_i2d: null object value");
    const unsigned char* data = CheckedData(object->data, object->length);
    const std::size_t length = static_cast<std::size_t>(object->length);
    if (length == 0) return {};
    return std::vector<unsigned char>(data, data + length);
}

std::vector<unsigned char> EncodeValue(Encoder encoder, void* value) {
    switch (encoder) {
    case Encoder::BitString:
        return Element(BitStringTag, BitStringContent(static_cast<const Asn1String*>(value)));
    case Encoder::OctetString:
        return Element(OctetStringTag, StringContent(static_cast<const Asn1String*>(value)));
    case Encoder::Integer:
        return Element(IntegerTag, IntegerContent(static_cast<const Asn1String*>(value)));
    case Encoder::Enumerated:
        return Element(EnumeratedTag, IntegerContent(static_cast<const Asn1String*>(value)));
    case Encoder::Ia5String:
        return Element(Ia5StringTag, StringContent(static_cast<const Asn1String*>(value)));
    case Encoder::GeneralizedTime:
        return Element(GeneralizedTimeTag, StringContent(static_cast<const Asn1String*>(value)));
    case Encoder::Object:
        return Element(ObjectTag, ObjectContent(static_cast<const LibreSslAsn1Object*>(value)));
    case Encoder::OcspNonce:
        return StringContent(static_cast<const Asn1String*>(value));
    case Encoder::Unsupported:
        break;
    }
    throw std::invalid_argument("X509_add1_ext_i2d: unsupported extension encoder");
}

char* CopyText(const char* text) {
    const std::size_t length = std::strlen(text) + 1;
    auto* copy = static_cast<char*>(malloc_nid_postfix(length));
    if (!copy) return nullptr;
    std::memcpy(copy, text, length);
    return copy;
}

LibreSslAsn1Object* DuplicateObject(const LibreSslAsn1Object* source) {
    auto* copy = static_cast<LibreSslAsn1Object*>(calloc_nid_postfix(1, sizeof(LibreSslAsn1Object)));
    if (!copy) return nullptr;
    copy->shortName = source->shortName;
    copy->longName = source->longName;
    copy->nid = source->nid;
    copy->length = 0;
    copy->data = nullptr;
    copy->flags = ObjectDynamic;
    if ((source->flags & ObjectDynamicStrings) != 0) {
        char* shortName = source->shortName ? CopyText(source->shortName) : nullptr;
        char* longName = source->longName ? CopyText(source->longName) : nullptr;
        if ((source->shortName && !shortName) || (source->longName && !longName)) {
            free_nid_postfix(shortName);
            free_nid_postfix(longName);
            free_nid_postfix(copy);
            return nullptr;
        }
        copy->shortName = shortName;
        copy->longName = longName;
        copy->flags |= ObjectDynamicStrings;
    }
    if (source->data) {
        if (source->length < 0) {
            ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(copy));
            throw std::invalid_argument("X509_add1_ext_i2d: negative object length");
        }
        if (source->length != 0) {
            auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(source->length)));
            if (!data) {
                ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(copy));
                return nullptr;
            }
            std::memcpy(data, source->data, static_cast<std::size_t>(source->length));
            copy->data = data;
            copy->length = source->length;
        }
        copy->flags |= ObjectDynamicData;
    }
    return copy;
}

LibreSsl3X509Extension* NewExtension(const LibreSslAsn1Object* source, int crit,
                                     const std::vector<unsigned char>& encoded) {
    auto* object = DuplicateObject(source);
    if (!object) return nullptr;
    auto* extension = static_cast<LibreSsl3X509Extension*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Extension)));
    if (!extension) {
        ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(const_cast<LibreSslAsn1Object*>(object)));
        return nullptr;
    }
    auto* value = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!value) {
        ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(const_cast<LibreSslAsn1Object*>(object)));
        free_nid_postfix(extension);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(calloc_nid_postfix(encoded.size() + 1, 1));
    if (!data) {
        ASN1_OCTET_STRING_free_nid_postfix(value);
        ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(const_cast<LibreSslAsn1Object*>(object)));
        free_nid_postfix(extension);
        return nullptr;
    }
    if (!encoded.empty()) std::memcpy(data, encoded.data(), encoded.size());
    value->length = static_cast<int>(encoded.size());
    value->type = OctetStringType;
    value->data = data;
    value->flags = 0;
    extension->object = object;
    extension->critical = crit ? 0xff : -1;
    extension->value = value;
    return extension;
}

LibreSslStack* StackOf(LibreSsl3ExtensionsX509* certificate) {
    auto* info = const_cast<LibreSsl3ExtensionsCertInfo*>(certificate->certInfo);
    auto* stack = reinterpret_cast<LibreSslStack*>(const_cast<LibreSsl3Stack*>(info->extensions));
    if (stack && stack->num < 0) throw std::invalid_argument("X509_add1_ext_i2d: negative extension count");
    return stack;
}

bool RemoveExtensionAt(LibreSslStack* stack, int position) {
    if (!stack || !stack->data || position < 0 || position >= stack->num) return false;
    auto* removed = reinterpret_cast<LibreSsl3X509Extension*>(stack->data[position]);
    if (position + 1 < stack->num) {
        std::memmove(&stack->data[position], &stack->data[position + 1],
                     static_cast<std::size_t>(stack->num - position - 1) * sizeof(char*));
    }
    --stack->num;
    stack->data[stack->num] = nullptr;
    X509_EXTENSION_free_nid_postfix(removed);
    return true;
}

}

extern "C" {

int APS5_VABI X509_add1_ext_i2d_nid_postfix(LibreSsl3ExtensionsX509* certificate, int nid, void* value,
                                             int crit, std::uint64_t flags) {
    if (!certificate) throw std::invalid_argument(std::string(__func__) + ": null certificate");
    auto* info = const_cast<LibreSsl3ExtensionsCertInfo*>(certificate->certInfo);
    if (!info) throw std::invalid_argument(std::string(__func__) + ": null certificate info");

    LibreSslStack* stack = StackOf(certificate);
    const std::uint64_t operation = flags & OperationMask;
    int position = -1;
    if (operation != AppendOperation) {
        position = X509_get_ext_by_NID_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(certificate), nid, -1);
        if (position == UnknownObjectPosition) {
            if ((flags & SilentFlag) == 0) PutError(UnknownExtensionReason);
            return 0;
        }
    }

    switch (operation) {
    case DefaultOperation:
        if (position >= 0) {
            if ((flags & SilentFlag) == 0) PutError(ExtensionExistsReason);
            return 0;
        }
        break;
    case AppendOperation:
        break;
    case ReplaceOperation:
        break;
    case ReplaceExistingOperation:
        if (position < 0) {
            if ((flags & SilentFlag) == 0) PutError(ExtensionNotFoundReason);
            return 0;
        }
        break;
    case KeepExistingOperation:
        if (position >= 0) return 1;
        break;
    case DeleteOperation:
        if (position < 0) {
            if ((flags & SilentFlag) == 0) PutError(ExtensionNotFoundReason);
            return 0;
        }
        return RemoveExtensionAt(stack, position) ? 1 : -1;
    default:
        if ((flags & SilentFlag) == 0) PutError(UnsupportedOptionReason);
        return -1;
    }

    const Method* method = FindMethod(nid);
    if (!method) {
        PutError(UnknownExtensionReason);
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }
    if (method->encoder == Encoder::Unsupported) NotImplemented_nid_no_patch(__func__);
    if (!value) {
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }

    std::vector<unsigned char> encoded = EncodeValue(method->encoder, value);
    if (method->encoder == Encoder::OcspNonce && encoded.empty()) {
        PutError(MallocFailureReason);
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        PutError(MallocFailureReason);
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }

    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (!object) {
        PutError(MallocFailureReason);
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }
    auto* extension = NewExtension(object, crit, encoded);
    if (!extension) {
        PutError(MallocFailureReason);
        PutError(ErrorCreatingExtensionReason);
        return 0;
    }

    if (position >= 0) {
        auto* replaced = reinterpret_cast<LibreSsl3X509Extension*>(stack->data[position]);
        stack->data[position] = reinterpret_cast<char*>(extension);
        X509_EXTENSION_free_nid_postfix(replaced);
        return 1;
    }

    bool created = false;
    if (!stack) {
        stack = sk_new_nid_postfix(nullptr);
        if (!stack) {
            X509_EXTENSION_free_nid_postfix(extension);
            return -1;
        }
        info->extensions = reinterpret_cast<const LibreSsl3Stack*>(stack);
        created = true;
    }
    if (!sk_push_nid_postfix(stack, extension)) {
        X509_EXTENSION_free_nid_postfix(extension);
        if (created) {
            sk_free_nid_postfix(stack);
            info->extensions = nullptr;
        }
        return -1;
    }
    return 1;
}

}
