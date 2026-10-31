#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, entries) == 0);

namespace {

struct Asn1Object {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

static_assert(sizeof(Asn1Object) == 40 && offsetof(Asn1Object, length) == 20 && offsetof(Asn1Object, data) == 24);

struct NameAttribute {
    int nid;
    int length;
    unsigned char der[10];
};

constexpr NameAttribute NameAttributes[] {
    {0, 0, {}},
    {13, 3, {0x55, 0x04, 0x03}},
    {14, 3, {0x55, 0x04, 0x06}},
    {15, 3, {0x55, 0x04, 0x07}},
    {16, 3, {0x55, 0x04, 0x08}},
    {17, 3, {0x55, 0x04, 0x0a}},
    {18, 3, {0x55, 0x04, 0x0b}},
    {48, 9, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01}},
    {99, 3, {0x55, 0x04, 0x2a}},
    {100, 3, {0x55, 0x04, 0x04}},
    {101, 3, {0x55, 0x04, 0x2b}},
    {105, 3, {0x55, 0x04, 0x05}},
    {106, 3, {0x55, 0x04, 0x0c}},
    {107, 3, {0x55, 0x04, 0x0d}},
    {173, 3, {0x55, 0x04, 0x29}},
    {174, 3, {0x55, 0x04, 0x2e}},
    {391, 10, {0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x19}},
    {400, 3, {0x55, 0x04, 0x48}},
    {458, 10, {0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x01}},
    {503, 3, {0x55, 0x04, 0x2d}},
    {509, 3, {0x55, 0x04, 0x2c}},
    {510, 3, {0x55, 0x04, 0x41}},
    {660, 3, {0x55, 0x04, 0x09}},
    {661, 3, {0x55, 0x04, 0x11}},
    {859, 3, {0x55, 0x04, 0x0e}},
    {860, 3, {0x55, 0x04, 0x0f}},
    {861, 3, {0x55, 0x04, 0x10}},
    {862, 3, {0x55, 0x04, 0x12}},
    {863, 3, {0x55, 0x04, 0x13}},
    {864, 3, {0x55, 0x04, 0x14}},
    {865, 3, {0x55, 0x04, 0x15}},
    {866, 3, {0x55, 0x04, 0x16}},
    {867, 3, {0x55, 0x04, 0x17}},
    {868, 3, {0x55, 0x04, 0x18}},
    {869, 3, {0x55, 0x04, 0x19}},
    {870, 3, {0x55, 0x04, 0x1a}},
    {871, 3, {0x55, 0x04, 0x1b}},
    {872, 3, {0x55, 0x04, 0x1c}},
    {873, 3, {0x55, 0x04, 0x1d}},
    {874, 3, {0x55, 0x04, 0x1e}},
    {875, 3, {0x55, 0x04, 0x1f}},
    {876, 3, {0x55, 0x04, 0x20}},
    {877, 3, {0x55, 0x04, 0x21}},
    {878, 3, {0x55, 0x04, 0x22}},
    {879, 3, {0x55, 0x04, 0x23}},
    {880, 3, {0x55, 0x04, 0x24}},
    {881, 3, {0x55, 0x04, 0x25}},
    {882, 3, {0x55, 0x04, 0x26}},
    {883, 3, {0x55, 0x04, 0x27}},
    {884, 3, {0x55, 0x04, 0x28}},
    {885, 3, {0x55, 0x04, 0x2f}},
    {886, 3, {0x55, 0x04, 0x30}},
    {887, 3, {0x55, 0x04, 0x31}},
    {888, 3, {0x55, 0x04, 0x32}},
    {889, 3, {0x55, 0x04, 0x33}},
    {890, 3, {0x55, 0x04, 0x34}},
    {891, 3, {0x55, 0x04, 0x35}},
    {892, 3, {0x55, 0x04, 0x36}},
};

const NameAttribute* FindAttribute(int nid, const char* function) {
    for (const auto& attribute : NameAttributes) {
        if (attribute.nid == nid) return &attribute;
    }
    NotImplemented_nid_no_patch((std::string(function) + ": NID " + std::to_string(nid) + " is not a name attribute type in the object identifier subset").c_str());
}

int IndexOf(const LibreSslX509Name* name, const NameAttribute& attribute, int lastpos, const char* function) {
    if (!name) return -1;
    if (lastpos == INT_MAX) throw std::invalid_argument(std::string(function) + ": last position overflows");
    if (lastpos < 0) lastpos = -1;
    const int count = name->entries ? name->entries->num : -1;
    for (++lastpos; lastpos < count; ++lastpos) {
        const auto* entry = X509_NAME_get_entry_nid_postfix(name, lastpos);
        if (!entry || !entry->object) throw std::invalid_argument(std::string(function) + ": null name entry or object");
        const auto* object = static_cast<const Asn1Object*>(entry->object);
        if (object->length != attribute.length) continue;
        if (attribute.length == 0) return lastpos;
        if (!object->data) throw std::invalid_argument(std::string(function) + ": null object data");
        if (std::memcmp(object->data, attribute.der, attribute.length) == 0) return lastpos;
    }
    return -1;
}

}

extern "C" {

LibreSslX509NameEntry* APS5_VABI X509_NAME_get_entry_nid_postfix(const LibreSslX509Name* name, int loc) {
    if (!name || loc < 0 || !name->entries || name->entries->num <= loc) return nullptr;
    if (!name->entries->data) throw std::invalid_argument(std::string(__func__) + ": null entry array");
    return reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[loc]);
}

void* APS5_VABI X509_NAME_ENTRY_get_data_nid_postfix(const LibreSslX509NameEntry* entry) {
    if (!entry) return nullptr;
    return entry->value;
}

int APS5_VABI X509_NAME_get_index_by_NID_nid_postfix(const LibreSslX509Name* name, int nid, int lastpos) {
    const auto* attribute = FindAttribute(nid, __func__);
    return IndexOf(name, *attribute, lastpos, __func__);
}

int APS5_VABI X509_NAME_get_text_by_NID_nid_postfix(const LibreSslX509Name* name, int nid, char* buf, int len) {
    const auto* attribute = FindAttribute(nid, __func__);
    const int index = IndexOf(name, *attribute, -1, __func__);
    if (index < 0) return -1;
    const auto* data = static_cast<const Asn1String*>(X509_NAME_ENTRY_get_data_nid_postfix(X509_NAME_get_entry_nid_postfix(name, index)));
    if (!data) throw std::invalid_argument(std::string(__func__) + ": null entry value");
    if (len == INT_MIN) throw std::invalid_argument(std::string(__func__) + ": buffer length overflows");
    const int count = data->length > len - 1 ? len - 1 : data->length;
    if (!buf) return data->length;
    if (count >= 0) {
        if (count) {
            if (!data->data) throw std::invalid_argument(std::string(__func__) + ": null entry string");
            std::memcpy(buf, data->data, count);
        }
        buf[count] = '\0';
    }
    return count;
}

}

namespace {

constexpr int GeneralString = 27;
constexpr int BufferLibrary = 7;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr std::size_t GrowthLimit = 0x5ffffffc;
constexpr std::size_t InitialBuffer = 200;
constexpr char Hex[] = "0123456789ABCDEF";

struct Buffer {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(Buffer) == 24 && offsetof(Buffer, data) == 8 && offsetof(Buffer, max) == 16);

char* GrowArray(char* data, std::size_t oldSize, std::size_t newSize) {
    if (!data) return static_cast<char*>(calloc_nid_postfix(newSize, 1));
    auto* grown = static_cast<char*>(malloc_nid_postfix(newSize));
    if (!grown) return nullptr;
    std::memcpy(grown, data, oldSize);
    std::memset(grown + oldSize, 0, newSize - oldSize);
    std::memset(data, 0, oldSize);
    free_nid_postfix(data);
    return grown;
}

bool Grow(Buffer* buffer, std::size_t length) {
    if (buffer->length >= length || buffer->max >= length) {
        buffer->length = length;
        return true;
    }
    if (length > GrowthLimit) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
        return false;
    }
    const std::size_t size = (length + 3) / 3 * 4;
    char* data = GrowArray(buffer->data, buffer->max, size);
    if (!data) {
        LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
        return false;
    }
    buffer->data = data;
    buffer->max = size;
    buffer->length = length;
    return true;
}

char* Fail(Buffer* buffer) {
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
    if (buffer) {
        if (buffer->data) {
            std::memset(buffer->data, 0, buffer->max);
            free_nid_postfix(buffer->data);
        }
        free_nid_postfix(buffer);
    }
    return nullptr;
}

} // namespace

extern "C" {

char* APS5_VABI X509_NAME_oneline_nid_postfix(const LibreSslX509Name* name, char* buffer, int length) {
    Buffer* grown = nullptr;
    if (!buffer) {
        grown = static_cast<Buffer*>(calloc_nid_postfix(1, sizeof(Buffer)));
        if (!grown) {
            LibreSslPutError_nid_no_patch(BufferLibrary, AnyFunction, MallocFailure);
            return Fail(nullptr);
        }
        if (!Grow(grown, InitialBuffer)) return Fail(grown);
        grown->data[0] = '\0';
        length = InitialBuffer;
    }
    if (!name) {
        if (grown) {
            buffer = grown->data;
            free_nid_postfix(grown);
        }
        constexpr char missing[] = "NO X509_NAME";
        const auto size = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(length));
        if (size > 0) {
            const std::size_t copied = std::min(sizeof(missing) - 1, size - 1);
            std::memcpy(buffer, missing, copied);
            buffer[copied] = '\0';
        }
        return buffer;
    }
    --length;
    int used = 0;
    int index = 0;
    char* out = nullptr;
    const int count = name->entries ? name->entries->num : -1;
    for (; index < count; ++index) {
        const LibreSslX509NameEntry* entry = X509_NAME_get_entry_nid_postfix(name, index);
        if (!entry || !entry->value) NotImplemented_nid_no_patch(__func__);
        const auto* object = static_cast<const LibreSslAsn1Object*>(entry->object);
        const auto* value = static_cast<const Asn1String*>(entry->value);
        const int nid = LibreSslObjectNid_nid_no_patch(object);
        const char* shortName = nullptr;
        char text[80];
        if (nid == 0 || !(shortName = LibreSslObjectShortName_nid_no_patch(nid))) {
            i2t_ASN1_OBJECT_nid_postfix(text, sizeof(text), object);
            shortName = text;
        }
        const int nameLength = static_cast<int>(std::strlen(shortName));
        const int type = value->type;
        const int bytes = value->length;
        const unsigned char* data = value->data;
        if (bytes > 0 && !data) NotImplemented_nid_no_patch(__func__);
        bool keep[4] = {true, true, true, true};
        if (type == GeneralString && bytes % 4 == 0) {
            bool nonzero[4] = {false, false, false, false};
            for (int position = 0; position < bytes; ++position) {
                if (data[position] != 0) nonzero[position & 3] = true;
            }
            if (!(nonzero[0] || nonzero[1] || nonzero[2])) keep[0] = keep[1] = keep[2] = false;
        }
        int valueLength = 0;
        for (int position = 0; position < bytes; ++position) {
            if (!keep[position & 3]) continue;
            ++valueLength;
            if (data[position] < ' ' || data[position] > '~') valueLength += 3;
        }
        const int previous = used;
        used += 1 + nameLength + 1 + valueLength;
        if (grown) {
            if (!Grow(grown, static_cast<std::size_t>(used) + 1)) return Fail(grown);
            out = grown->data + previous;
        } else if (used > length) {
            break;
        } else {
            out = buffer + previous;
        }
        *out++ = '/';
        std::memcpy(out, shortName, static_cast<std::size_t>(nameLength));
        out += nameLength;
        *out++ = '=';
        for (int position = 0; position < bytes; ++position) {
            if (!keep[position & 3]) continue;
            const unsigned char character = data[position];
            if (character < ' ' || character > '~') {
                *out++ = '\\';
                *out++ = 'x';
                *out++ = Hex[(character >> 4) & 0x0f];
                *out++ = Hex[character & 0x0f];
            } else {
                *out++ = static_cast<char>(character);
            }
        }
        *out = '\0';
    }
    char* result;
    if (grown) {
        result = grown->data;
        free_nid_postfix(grown);
    } else {
        result = buffer;
    }
    if (index == 0) *result = '\0';
    return result;
}

}
