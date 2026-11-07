#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "prx/libSceLibreSsl/include/Bio.hpp"

namespace {

using LibreSsl::Bio;

constexpr int BufferLibrary = 7;
constexpr int BioLibrary = 32;
constexpr int PemLibrary = 9;
constexpr int ErrorFunctionUnknown = 0xfff;
constexpr int ErrorMallocFailure = 65;
constexpr int BioUninitialized = 120;
constexpr int BioUnsupportedMethod = 121;
constexpr int PemNoStartLine = 108;
constexpr int BioCallbackGets = 0x05;
constexpr int BioCallbackReturn = 0x80;

struct PemBuffer {
    std::size_t length;
    char* data;
    std::size_t capacity;
};

static_assert(sizeof(PemBuffer) == 24);

void PutBufferError() {
    LibreSslPutError_nid_no_patch(BufferLibrary, ErrorFunctionUnknown, ErrorMallocFailure, __FILE__, __LINE__);
}

void PutPemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, ErrorFunctionUnknown, reason, __FILE__, __LINE__);
}

PemBuffer* NewPemBuffer() {
    auto* const buffer = static_cast<PemBuffer*>(calloc_nid_postfix(1, sizeof(PemBuffer)));
    if (buffer == nullptr) PutBufferError();
    return buffer;
}

void FreePemBuffer(PemBuffer* buffer) {
    if (buffer == nullptr) return;
    if (buffer->data != nullptr) {
        auto* const bytes = reinterpret_cast<volatile unsigned char*>(buffer->data);
        for (std::size_t index = 0; index < buffer->capacity; ++index) bytes[index] = 0;
        free_nid_postfix(buffer->data);
    }
    free_nid_postfix(buffer);
}

struct PemBuffers {
    PemBuffer* name = nullptr;
    PemBuffer* header = nullptr;
    PemBuffer* data = nullptr;

    void Clear() {
        FreePemBuffer(name);
        FreePemBuffer(header);
        FreePemBuffer(data);
        name = header = data = nullptr;
    }
    ~PemBuffers() { Clear(); }
};

bool GrowPemBuffer(PemBuffer* buffer, std::size_t length) {
    if (buffer->length >= length || buffer->capacity >= length) {
        buffer->length = length;
        return true;
    }
    if (length > 0x5ffffffc) {
        PutBufferError();
        return false;
    }
    const std::size_t capacity = ((length + 3) / 3) * 4;
    auto* const grown = static_cast<char*>(recallocarray_nid_postfix(buffer->data, buffer->capacity, capacity, 1));
    if (grown == nullptr) {
        PutBufferError();
        return false;
    }
    buffer->data = grown;
    buffer->capacity = capacity;
    buffer->length = length;
    return true;
}

int BioGets(Bio* bio, char* buffer, int size) {
    if (bio == nullptr || bio->method == nullptr || bio->method->gets == nullptr) {
        LibreSslPutError_nid_no_patch(BioLibrary, ErrorFunctionUnknown, BioUnsupportedMethod, __FILE__, __LINE__);
        return -2;
    }
    const auto callback = bio->callback;
    int result = 0;
    if (callback && (result = static_cast<int>(callback(bio, BioCallbackGets, buffer, size, 0, 1))) <= 0) return result;
    if (!bio->initialized) {
        LibreSslPutError_nid_no_patch(BioLibrary, ErrorFunctionUnknown, BioUninitialized, __FILE__, __LINE__);
        return -2;
    }
    result = bio->method->gets(bio, buffer, size);
    if (callback) result = static_cast<int>(callback(bio, BioCallbackGets | BioCallbackReturn, buffer, size, 0, result));
    return result;
}

bool ReadLine(Bio* bio, std::array<char, 256>& line, std::size_t& length) {
    line[254] = '\0';
    const int count = BioGets(bio, line.data(), 254);
    if (count <= 0) return false;
    if (count > 254) throw std::invalid_argument("PEM_read_bio_X509 BIO_gets returned an oversized line");
    int index = count;
    while (index >= 0 && line[static_cast<std::size_t>(index)] <= ' ') --index;
    line[static_cast<std::size_t>(++index)] = '\n';
    line[static_cast<std::size_t>(++index)] = '\0';
    length = static_cast<std::size_t>(index);
    return true;
}

bool IsBeginLine(const std::array<char, 256>& line, std::size_t length) {
    constexpr char prefix[] = "-----BEGIN ";
    constexpr char suffix[] = "-----\n";
    if (length < sizeof(prefix) - 1 || std::memcmp(line.data(), prefix, sizeof(prefix) - 1) != 0) return false;
    if (length < sizeof(prefix) - 1 + sizeof(suffix) - 1)
        throw std::invalid_argument("PEM_read_bio_X509 malformed BEGIN line");
    return std::memcmp(line.data() + length - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1) == 0;
}

}

extern "C" {

LibreSslX509* APS5_VABI PEM_read_bio_X509_nid_postfix(LibreSsl::Bio* bio, LibreSslX509** certificate,
                                                      LibreSslPemPasswordCallback callback, void* userData) {
    static_cast<void>(certificate);
    static_cast<void>(callback);
    static_cast<void>(userData);

    PemBuffers buffers;
    buffers.name = NewPemBuffer();
    buffers.header = NewPemBuffer();
    buffers.data = NewPemBuffer();
    if (buffers.name == nullptr || buffers.header == nullptr || buffers.data == nullptr) {
        buffers.Clear();
        PutPemError(ErrorMallocFailure);
        return nullptr;
    }

    std::array<char, 256> line{};
    std::size_t lineLength = 0;
    for (;;) {
        if (!ReadLine(bio, line, lineLength)) {
            PutPemError(PemNoStartLine);
            return nullptr;
        }
        lineLength = std::strlen(line.data());
        if (!IsBeginLine(line, lineLength)) continue;
        const std::size_t nameLength = lineLength - 11 - 6;
        if (!GrowPemBuffer(buffers.name, lineLength - 11 + 9)) {
            PutPemError(ErrorMallocFailure);
            return nullptr;
        }
        std::memcpy(buffers.name->data, line.data() + 11, nameLength);
        buffers.name->data[nameLength] = '\0';
        if (!GrowPemBuffer(buffers.header, 256)) {
            PutPemError(ErrorMallocFailure);
            return nullptr;
        }
        buffers.header->data[0] = '\0';
        NotImplemented_nid_no_patch(__func__);
    }
}

}
