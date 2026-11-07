#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr int GeneralString = 27;
constexpr int BufferLibrary = 7;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr std::size_t GrowthLimit = 0x5ffffffc;
constexpr std::size_t InitialBuffer = 200;
constexpr char Hex[] = "0123456789ABCDEF";

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    void* comp;
};

struct NameEntry {
    LibreSslAsn1Object* object;
    Asn1String* value;
    std::int32_t set;
    std::int32_t size;
};

struct Name {
    Stack* entries;
    std::int32_t modified;
    void* bytes;
    unsigned char* canonicalEncoding;
    std::int32_t canonicalLength;
};

struct Buffer {
    std::uint64_t length;
    char* data;
    std::uint64_t max;
};

static_assert(sizeof(Stack) == 32 && offsetof(Stack, data) == 8);
static_assert(sizeof(NameEntry) == 24 && offsetof(NameEntry, value) == 8);
static_assert(sizeof(Name) == 40 && offsetof(Name, entries) == 0);
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

NameEntry* EntryAt(const Stack* entries, int index, const char* function) {
    if (!entries || index < 0 || index >= entries->num) return nullptr;
    if (!entries->data) NotImplemented_nid_no_patch(function);
    return reinterpret_cast<NameEntry*>(entries->data[index]);
}

} // namespace

extern "C" {

char* APS5_VABI X509_NAME_oneline_nid_postfix(const void* name, char* buffer, int length) {
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
    const auto* entries = static_cast<const Name*>(name)->entries;
    --length;
    int used = 0;
    int index = 0;
    char* out = nullptr;
    const int count = entries ? entries->num : -1;
    for (; index < count; ++index) {
        const NameEntry* entry = EntryAt(entries, index, __func__);
        if (!entry || !entry->value) NotImplemented_nid_no_patch(__func__);
        const int nid = LibreSslObjectNid_nid_no_patch(entry->object);
        const char* shortName = nullptr;
        char text[80];
        if (nid == 0 || !(shortName = LibreSslObjectShortName_nid_no_patch(nid))) {
            i2t_ASN1_OBJECT_nid_postfix(text, sizeof(text), entry->object);
            shortName = text;
        }
        const int nameLength = static_cast<int>(std::strlen(shortName));
        const int type = entry->value->type;
        const int bytes = entry->value->length;
        const unsigned char* data = entry->value->data;
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
