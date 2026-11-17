#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

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

template <typename T>
T CopyGuestInPreservingErrno(const T* source, const char* function) {
    T destination{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return destination;
}

void CopyGuestBytesInPreservingErrno(void* destination, const unsigned char* source, std::size_t size, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

std::vector<unsigned char> CopyGuestBytesInPreservingErrno(const unsigned char* source, int length, const char* function) {
    const std::size_t size = length > 0 ? static_cast<std::size_t>(length) : 0;
    unsigned char chunk[4096];
    for (std::size_t done = 0; done < size; done += sizeof(chunk))
        CopyGuestBytesInPreservingErrno(chunk, source + done, std::min(size - done, sizeof(chunk)), function);
    std::vector<unsigned char> bytes(size);
    CopyGuestBytesInPreservingErrno(bytes.data(), source, size, function);
    return bytes;
}

void CopyGuestBytesOutPreservingErrno(void* destination, const void* source, std::size_t size, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

const NameEntry* EntryAt(const Stack& entries, int index, const char* function) {
    if (index < 0 || index >= entries.num) return nullptr;
    if (!entries.data) throw std::invalid_argument(function);
    return reinterpret_cast<const NameEntry*>(CopyGuestInPreservingErrno(entries.data + index, function));
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
            std::string written(missing, copied);
            CopyGuestBytesOutPreservingErrno(buffer, written.c_str(), copied + 1, __func__);
        }
        return buffer;
    }
    const auto* entries = CopyGuestInPreservingErrno(static_cast<const Name*>(name), __func__).entries;
    const Stack stack = entries ? CopyGuestInPreservingErrno(entries, __func__) : Stack{};
    --length;
    int used = 0;
    int index = 0;
    std::string written;
    const int count = entries ? stack.num : -1;
    for (; index < count; ++index) {
        const NameEntry* address = EntryAt(stack, index, __func__);
        if (!address) throw std::invalid_argument("X509_NAME_oneline: null name entry or value");
        const NameEntry entry = CopyGuestInPreservingErrno(address, __func__);
        if (!entry.value) throw std::invalid_argument("X509_NAME_oneline: null name entry or value");
        LibreSslAsn1Object object{};
        const LibreSslAsn1Object* copied = nullptr;
        if (entry.object) {
            object = CopyGuestInPreservingErrno(entry.object, __func__);
            copied = &object;
        }
        std::vector<unsigned char> objectData;
        const auto copyObjectData = [&] {
            if (!object.data || object.length <= 0 || !objectData.empty()) return;
            objectData = CopyGuestBytesInPreservingErrno(object.data, object.length, __func__);
            object.data = objectData.data();
        };
        if (object.nid == 0) copyObjectData();
        const int nid = LibreSslObjectNid_nid_no_patch(copied);
        const char* shortName = nullptr;
        char text[80];
        if (nid == 0 || !(shortName = LibreSslObjectShortName_nid_no_patch(nid))) {
            copyObjectData();
            i2t_ASN1_OBJECT_nid_postfix(text, sizeof(text), copied);
            shortName = text;
        }
        const int nameLength = static_cast<int>(std::strlen(shortName));
        const Asn1String value = CopyGuestInPreservingErrno(entry.value, __func__);
        const int type = value.type;
        const int bytes = value.length;
        if (bytes > 0 && !value.data) throw std::invalid_argument("X509_NAME_oneline: null value data");
        const std::vector<unsigned char> data = CopyGuestBytesInPreservingErrno(value.data, bytes, __func__);
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
        used += 1 + nameLength + 1 + valueLength;
        if (grown) {
            if (!Grow(grown, static_cast<std::size_t>(used) + 1)) return Fail(grown);
        } else if (used > length) {
            break;
        }
        written += '/';
        written.append(shortName, static_cast<std::size_t>(nameLength));
        written += '=';
        for (int position = 0; position < bytes; ++position) {
            if (!keep[position & 3]) continue;
            const unsigned char character = data[position];
            if (character < ' ' || character > '~') {
                written += '\\';
                written += 'x';
                written += Hex[(character >> 4) & 0x0f];
                written += Hex[character & 0x0f];
            } else {
                written += static_cast<char>(character);
            }
        }
    }
    char* result;
    if (grown) {
        result = grown->data;
        free_nid_postfix(grown);
    } else {
        result = buffer;
    }
    CopyGuestBytesOutPreservingErrno(result, written.c_str(), written.size() + 1, __func__);
    return result;
}

}
