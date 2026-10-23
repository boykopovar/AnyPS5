#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>

extern "C" {
void* APS5_VABI calloc_nid_postfix(std::size_t, std::size_t);
void APS5_VABI free_nid_postfix(void*);
}

namespace {

using LibreSsl::Bio;
using LibreSsl::BioBuffer;
using LibreSsl::BioMethod;

constexpr int ReadOnly = 0x200;
constexpr int RetryFlags = 0xf;
constexpr int RetryRead = 0x9;
constexpr std::size_t MaximumGrowth = 0x5ffffffc;
constexpr int BioLibrary = 32;
constexpr int BufferLibrary = 7;
constexpr int AllocationFailure = 65;
constexpr int NullParameter = 115;
constexpr int Uninitialized = 120;
constexpr int UnsupportedMethod = 121;
constexpr int ReadOnlyWrite = 126;

int queueError(int library, int reason, int result) {
    LibreSslPutError_nid_no_patch(library, 0xfff, reason, __FILE__, __LINE__);
    return result;
}

void clearBuffer(BioBuffer& buffer) {
    auto* bytes = reinterpret_cast<volatile unsigned char*>(buffer.data);
    for (std::size_t i = 0; i < buffer.capacity; ++i) bytes[i] = 0;
}

BioBuffer& memoryBuffer(Bio* bio) {
    if (!bio || !bio->data) throw std::invalid_argument("BIO: missing memory buffer");
    return *static_cast<BioBuffer*>(bio->data);
}

int APS5_VABI createMemory(Bio* bio) {
    auto* buffer = static_cast<BioBuffer*>(calloc_nid_postfix(1, sizeof(BioBuffer)));
    if (!buffer) return queueError(BufferLibrary, AllocationFailure, 0);
    bio->data = buffer;
    bio->initialized = 1;
    bio->shutdown = 1;
    bio->number = -1;
    return 1;
}

int APS5_VABI destroyMemory(Bio* bio) {
    if (!bio) return 0;
    if (bio->shutdown && bio->initialized && bio->data) {
        auto* buffer = static_cast<BioBuffer*>(bio->data);
        if (!(bio->flags & ReadOnly)) {
            clearBuffer(*buffer);
            free_nid_postfix(buffer->data);
        }
        free_nid_postfix(buffer);
        bio->data = nullptr;
    }
    return 1;
}

int APS5_VABI readMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    bio->flags &= ~RetryFlags;
    const int count = size >= 0 ? static_cast<int>(std::min<std::size_t>(size, buffer.length)) : size;
    if (destination && count > 0) {
        std::memcpy(destination, buffer.data, count);
        buffer.length -= count;
        if (bio->flags & ReadOnly) buffer.data += count;
        else std::memmove(buffer.data, buffer.data + count, buffer.length);
        return count;
    }
    if (buffer.length != 0) return count;
    if (bio->number != 0) bio->flags |= RetryRead;
    return bio->number;
}

int APS5_VABI writeMemory(Bio* bio, const char* source, int size) {
    auto& buffer = memoryBuffer(bio);
    if (!source) return queueError(BioLibrary, NullParameter, -1);
    if (bio->flags & ReadOnly) return queueError(BioLibrary, ReadOnlyWrite, -1);
    if (size < 0) throw std::invalid_argument("BIO: negative memory write size");
    bio->flags &= ~RetryFlags;
    const auto count = static_cast<std::size_t>(size);
    if (buffer.length > std::numeric_limits<std::size_t>::max() - count) NotImplemented_nid_no_patch(__func__);
    const std::size_t length = buffer.length + count;
    if (length > buffer.capacity) {
        if (length > MaximumGrowth) return queueError(BufferLibrary, AllocationFailure, -1);
        const std::size_t capacity = (length + 3) / 3 * 4;
        auto* data = static_cast<char*>(calloc_nid_postfix(capacity, 1));
        if (!data) return queueError(BufferLibrary, AllocationFailure, -1);
        if (buffer.length != 0) std::memcpy(data, buffer.data, buffer.length);
        if (count != 0) std::memcpy(data + buffer.length, source, count);
        clearBuffer(buffer);
        free_nid_postfix(buffer.data);
        buffer.data = data;
        buffer.capacity = capacity;
    } else if (count != 0) {
        std::memmove(buffer.data + buffer.length, source, count);
    }
    buffer.length = length;
    return size;
}

int APS5_VABI putsMemory(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("BIO_puts: null string");
    const std::size_t size = std::strlen(source);
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) NotImplemented_nid_no_patch(__func__);
    return writeMemory(bio, source, static_cast<int>(size));
}

int APS5_VABI getsMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    if (!destination || size == std::numeric_limits<int>::min()) throw std::invalid_argument("BIO: invalid line buffer");
    bio->flags &= ~RetryFlags;
    if (size <= 1 || buffer.length == 0) {
        *destination = '\0';
        return 0;
    }
    const auto limit = std::min<std::size_t>(size - 1, buffer.length);
    const void* newline = std::memchr(buffer.data, '\n', limit);
    const std::size_t count = newline ? static_cast<const char*>(newline) - buffer.data + 1 : limit;
    const int result = readMemory(bio, destination, static_cast<int>(count));
    destination[result] = '\0';
    return result;
}

std::int64_t APS5_VABI controlMemory(Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) throw std::invalid_argument("BIO: null memory BIO");
    switch (command) {
    case 1: {
        auto& buffer = memoryBuffer(bio);
        if (buffer.data) {
            if (bio->flags & ReadOnly) {
                if (buffer.capacity < buffer.length) throw std::invalid_argument("BIO: invalid read-only buffer length");
                buffer.data -= buffer.capacity - buffer.length;
                buffer.length = buffer.capacity;
            } else {
                clearBuffer(buffer);
                buffer.length = 0;
            }
        }
        return 1;
    }
    case 2:
        return memoryBuffer(bio).length == 0;
    case 3:
        if (pointer) *static_cast<char**>(pointer) = memoryBuffer(bio).data;
        return static_cast<std::int64_t>(memoryBuffer(bio).length);
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 10:
        return static_cast<std::int64_t>(memoryBuffer(bio).length);
    case 11:
    case 12:
        return 1;
    case 13:
        return 0;
    case 114:
        destroyMemory(bio);
        bio->data = pointer;
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 115:
        if (pointer) std::memcpy(pointer, &bio->data, sizeof(bio->data));
        return 1;
    case 130:
        bio->number = static_cast<int>(argument);
        return 1;
    default:
        return 0;
    }
}

const BioMethod MemoryMethod{0x401, "memory buffer", writeMemory, readMemory, putsMemory, getsMemory, controlMemory, createMemory, destroyMemory, nullptr};

}  // namespace

extern "C" {

const LibreSsl::BioMethod* APS5_VABI BIO_s_mem_nid_postfix() {
    return &MemoryMethod;
}

LibreSsl::Bio* APS5_VABI BIO_new_nid_postfix(const LibreSsl::BioMethod* method) {
    if (!method) throw std::invalid_argument("BIO_new: null method");
    auto* bio = static_cast<Bio*>(calloc_nid_postfix(1, sizeof(Bio)));
    if (!bio) {
        queueError(BioLibrary, AllocationFailure, 0);
        return nullptr;
    }
    bio->method = method;
    bio->shutdown = 1;
    bio->references = 1;
    try {
        if (!method->create || method->create(bio)) return bio;
    } catch (...) {
        free_nid_postfix(bio);
        throw;
    }
    free_nid_postfix(bio);
    return nullptr;
}

LibreSsl::Bio* APS5_VABI BIO_new_mem_buf_nid_postfix(const void* source, int size) {
    if (!source) {
        queueError(BioLibrary, NullParameter, 0);
        return nullptr;
    }
    auto* bio = BIO_new_nid_postfix(&MemoryMethod);
    if (!bio) return nullptr;
    auto& buffer = memoryBuffer(bio);
    buffer.data = const_cast<char*>(static_cast<const char*>(source));
    buffer.length = size < 0 ? std::strlen(buffer.data) : static_cast<std::size_t>(size);
    buffer.capacity = buffer.length;
    bio->flags |= ReadOnly;
    bio->number = 0;
    return bio;
}

int APS5_VABI BIO_free_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) return 0;
    if (bio->extraData) NotImplemented_nid_no_patch(__func__);
    if (std::atomic_ref(bio->references).fetch_sub(1) > 1) return 1;
    if (bio->callback) {
        const int result = static_cast<int>(bio->callback(bio, 1, nullptr, 0, 0, 1));
        if (result <= 0) return result;
    }
    if (bio->extraData) NotImplemented_nid_no_patch(__func__);
    if (bio->method && bio->method->destroy) bio->method->destroy(bio);
    free_nid_postfix(bio);
    return 1;
}

std::int64_t APS5_VABI BIO_ctrl_nid_postfix(LibreSsl::Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) return 0;
    if (!bio->method || !bio->method->control) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const auto result = callback(bio, 6, static_cast<const char*>(pointer), command, argument, 1);
        if (result <= 0) return result;
    }
    const auto result = bio->method->control(bio, command, argument, pointer);
    return callback ? callback(bio, 0x86, static_cast<const char*>(pointer), command, argument, result) : result;
}

int LibreSslBioWrite_nid_no_patch(LibreSsl::Bio* bio, const void* source, int size) {
    if (!bio || !source || size <= 0) return 0;
    if (!bio->method || !bio->method->write) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, 3, static_cast<const char*>(source), size, 0, 1));
        if (result <= 0) return result;
    }
    if (!bio->initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = bio->method->write(bio, static_cast<const char*>(source), size);
    if (result > 0) bio->bytesWritten += static_cast<std::uint64_t>(result);
    return callback ? static_cast<int>(callback(bio, 0x83, static_cast<const char*>(source), size, 0, result)) : result;
}

int APS5_VABI BIO_puts_nid_postfix(LibreSsl::Bio* bio, const char* source) {
    if (!bio || !bio->method || !bio->method->puts) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, 4, source, 0, 0, 1));
        if (result <= 0) return result;
    }
    if (!bio->initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = bio->method->puts(bio, source);
    if (result > 0) bio->bytesWritten += static_cast<std::uint64_t>(result);
    return callback ? static_cast<int>(callback(bio, 0x84, source, 0, 0, result)) : result;
}

}
