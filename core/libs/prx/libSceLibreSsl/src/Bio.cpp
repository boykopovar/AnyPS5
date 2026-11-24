#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>


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
constexpr int SslLibrary = 20;
constexpr int SslBioType = 0x207;
constexpr int SslBioSetSsl = 109;
constexpr int SslBioGetSsl = 110;
constexpr int SslBioGetRenegotiates = 126;
constexpr int SslBioGetClose = 8;
constexpr int SslBioSetClose = 9;
constexpr int SslBioInfo = 3;
constexpr int SslBioSetCallback = 14;

struct BioSslData {
    void* ssl;
    int renegotiates;
    std::uint64_t renegotiateCount;
    std::uint64_t byteCount;
    std::uint64_t renegotiateTimeout;
    std::int64_t lastTime;
};

static_assert(sizeof(BioSslData) == 48);
static_assert(offsetof(BioSslData, ssl) == 0);
static_assert(offsetof(BioSslData, renegotiates) == 8);
static_assert(offsetof(BioSslData, renegotiateCount) == 16);
static_assert(offsetof(BioSslData, byteCount) == 24);
static_assert(offsetof(BioSslData, renegotiateTimeout) == 32);
static_assert(offsetof(BioSslData, lastTime) == 40);

int queueError(int library, int reason, int result) {
    LibreSslPutError_nid_no_patch(library, 0xfff, reason, __FILE__, __LINE__);
    return result;
}

template <typename T>
bool ReadGuest(T& destination, const T* source) {
    return GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
}

template <typename T>
void CopyGuestIn(T& destination, const T* source, const char* caller) {
    if (!ReadGuest(destination, source)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

template <typename T>
void RequireGuestWritable(T* guest, const char* caller) {
    if (!GuestWritable_nid_no_patch(guest, sizeof(T))) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

Bio ReadWritableBio(Bio* bio, const char* caller) {
    Bio guestBio{};
    CopyGuestIn(guestBio, bio, caller);
    RequireGuestWritable(bio, caller);
    return guestBio;
}

BioMethod ReadBioMethod(const BioMethod* method, const char* caller) {
    BioMethod guestMethod{};
    CopyGuestIn(guestMethod, method, caller);
    return guestMethod;
}

int DecrementReferences(Bio* bio) {
    auto* const field = reinterpret_cast<int*>(reinterpret_cast<std::byte*>(bio) + offsetof(Bio, references));
    return std::atomic_ref(*field).fetch_sub(1);
}

void AddBytesWritten(Bio* bio, std::uint64_t delta, const char* caller) {
    auto* const field = reinterpret_cast<std::byte*>(bio) + offsetof(Bio, bytesWritten);
    std::uint64_t bytesWritten = 0;
    if (!GuestCopyIn_nid_no_patch(&bytesWritten, field, sizeof(bytesWritten)))
        NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    bytesWritten += delta;
    if (!GuestCopyOut_nid_no_patch(field, &bytesWritten, sizeof(bytesWritten)))
        NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

bool CopyGuestStringPreservingErrno(const char* source, std::string& destination) {
    constexpr std::uintptr_t Chunk = 4096;
    char bytes[Chunk];
    destination.clear();
    for (auto address = reinterpret_cast<std::uintptr_t>(source);;) {
        const std::size_t count = Chunk - address % Chunk;
        if (!GuestCopyIn_nid_no_patch(bytes, reinterpret_cast<const void*>(address), count)) return false;
        const auto* end = static_cast<const char*>(std::memchr(bytes, 0, count));
        destination.append(bytes, end ? static_cast<std::size_t>(end - bytes) : count);
        if (end) return true;
        address += count;
    }
}

void clearBuffer(BioBuffer& buffer) {
    if (!buffer.data || buffer.capacity == 0) return;
    if (!GuestWritable_nid_no_patch(buffer.data, buffer.capacity))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto* bytes = reinterpret_cast<volatile unsigned char*>(buffer.data);
    for (std::size_t i = 0; i < buffer.capacity; ++i) bytes[i] = 0;
}

BioBuffer& memoryBuffer(Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO: missing memory buffer");
    const Bio guestBio = ReadWritableBio(bio, __func__);
    if (!guestBio.data) throw std::invalid_argument("BIO: missing memory buffer");
    auto* const buffer = static_cast<BioBuffer*>(guestBio.data);
    BioBuffer guestBuffer{};
    CopyGuestIn(guestBuffer, buffer, __func__);
    RequireGuestWritable(buffer, __func__);
    return *buffer;
}

int APS5_VABI createMemory(Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO: missing memory buffer");
    RequireGuestWritable(bio, __func__);
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
    const Bio guestBio = ReadWritableBio(bio, __func__);
    if (guestBio.shutdown && guestBio.initialized && guestBio.data) {
        auto& buffer = memoryBuffer(bio);
        if (!(guestBio.flags & ReadOnly)) {
            clearBuffer(buffer);
            free_nid_postfix(buffer.data);
        }
        free_nid_postfix(&buffer);
        bio->data = nullptr;
    }
    return 1;
}

BioSslData& sslData(Bio* bio) {
    if (!bio) throw std::invalid_argument("SSL BIO: missing state");
    const Bio guestBio = ReadWritableBio(bio, __func__);
    if (!guestBio.data) throw std::invalid_argument("SSL BIO: missing state");
    auto* const state = static_cast<BioSslData*>(guestBio.data);
    BioSslData guestState{};
    CopyGuestIn(guestState, state, __func__);
    RequireGuestWritable(state, __func__);
    return *state;
}

int APS5_VABI readSsl(Bio*, char* destination, int) {
    if (!destination) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI writeSsl(Bio*, const char* source, int) {
    if (!source) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putsSsl(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("SSL BIO puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeSsl(bio, text.c_str(), static_cast<int>(text.size()));
}

std::int64_t APS5_VABI controlSsl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    auto& state = sslData(bio);
    if (!state.ssl && command != SslBioSetSsl) return 0;
    switch (command) {
    case SslBioSetSsl:
        if (state.ssl || pointer) NotImplemented_nid_no_patch(__func__);
        bio->shutdown = static_cast<int>(argument);
        state.ssl = nullptr;
        throw std::invalid_argument("SSL BIO control: null SSL");
    case SslBioInfo:
        return 0;
    case SslBioGetSsl:
        if (!pointer) return 0;
        if (!GuestCopyOut_nid_no_patch(pointer, &state.ssl, sizeof(state.ssl))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 1;
    case SslBioGetClose:
        return bio->shutdown;
    case SslBioSetClose:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case SslBioGetRenegotiates:
        return state.renegotiates;
    default:
        NotImplemented_nid_no_patch(__func__);
    }
}

int APS5_VABI createSsl(Bio* bio) {
    if (!bio) throw std::invalid_argument("SSL BIO: missing state");
    RequireGuestWritable(bio, __func__);
    auto* state = static_cast<BioSslData*>(calloc_nid_postfix(1, sizeof(BioSslData)));
    if (!state) return queueError(SslLibrary, AllocationFailure, 0);
    bio->initialized = 0;
    bio->data = state;
    bio->flags = 0;
    return 1;
}

int APS5_VABI destroySsl(Bio* bio) {
    if (!bio) return 0;
    const Bio guestBio = ReadWritableBio(bio, __func__);
    auto& state = sslData(bio);
    if (state.ssl) NotImplemented_nid_no_patch(__func__);
    if (guestBio.shutdown) {
        bio->initialized = 0;
        bio->flags = 0;
    }
    free_nid_postfix(&state);
    return 1;
}

template <typename TMethod, typename TInvoker>
std::int64_t DispatchControl(Bio* bio, int command, std::int64_t argument, const char* callbackPointer,
                             TMethod BioMethod::* methodField, TInvoker invoke, const char* caller) {
    if (!bio) return 0;
    const Bio guestBio = ReadWritableBio(bio, caller);
    if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
    const BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
    const auto methodFunction = guestMethod.*methodField;
    if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = guestBio.callback;
    if (callback) {
        const auto result = callback(bio, 6, callbackPointer, command, argument, 1);
        if (result <= 0) return result;
    }
    const auto result = invoke(methodFunction, bio);
    return callback ? callback(bio, 0x86, callbackPointer, command, argument, result) : result;
}

template <typename TMethod, typename TInvoker>
int DispatchOutput(Bio* bio, int operation, const char* source, int size,
                   TMethod BioMethod::* methodField, TInvoker invoke, const char* caller) {
    Bio guestBio = ReadWritableBio(bio, caller);
    if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
    BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
    auto methodFunction = guestMethod.*methodField;
    if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = guestBio.callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, operation, source, size, 0, 1));
        if (result <= 0) return result;
        guestBio = ReadWritableBio(bio, caller);
        if (!guestBio.method) return queueError(BioLibrary, UnsupportedMethod, -2);
        guestMethod = ReadBioMethod(guestBio.method, caller);
        methodFunction = guestMethod.*methodField;
        if (!methodFunction) return queueError(BioLibrary, UnsupportedMethod, -2);
    }
    if (!guestBio.initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = invoke(methodFunction, bio);
    if (result > 0) AddBytesWritten(bio, static_cast<std::uint64_t>(result), caller);
    return callback ? static_cast<int>(callback(bio, operation | 0x80, source, size, 0, result)) : result;
}

std::int64_t forwardCallbackControl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    return DispatchControl(bio, command, 0, reinterpret_cast<const char*>(&callback),
        &BioMethod::callbackControl,
        [command, callback](auto fn, Bio* target) { return fn(target, command, callback); },
        __func__);
}

std::int64_t APS5_VABI callbackControlSsl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    auto& state = sslData(bio);
    if (!state.ssl) throw std::invalid_argument("SSL BIO callback control: null SSL");
    LibreSslSsl ssl{};
    CopyGuestIn(ssl, static_cast<const LibreSslSsl*>(state.ssl), __func__);
    if (command != SslBioSetCallback) return forwardCallbackControl(static_cast<Bio*>(ssl.rbio), command, callback);
    if (!ssl.internal) throw std::invalid_argument("SSL BIO callback control: null internal state");
    const auto value = reinterpret_cast<decltype(ssl.internal->infoCallback)>(callback);
    if (!GuestCopyOut_nid_no_patch(&ssl.internal->infoCallback, &value, sizeof(value)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return 1;
}

void consumeMemory(Bio* bio, BioBuffer& buffer, std::size_t count) {
    buffer.length -= count;
    if (bio->flags & ReadOnly) buffer.data += count;
    else std::memmove(buffer.data, buffer.data + count, buffer.length);
}

int APS5_VABI readMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    bio->flags &= ~RetryFlags;
    const int count = size >= 0 ? static_cast<int>(std::min<std::size_t>(size, buffer.length)) : size;
    if (destination && count > 0) {
        std::vector<char> bytes(static_cast<std::size_t>(count));
        if (!GuestCopyIn_nid_no_patch(bytes.data(), buffer.data, bytes.size())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        if (!GuestCopyOut_nid_no_patch(destination, bytes.data(), bytes.size())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        consumeMemory(bio, buffer, static_cast<std::size_t>(count));
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
        if (count != 0 && !GuestCopyIn_nid_no_patch(data + buffer.length, source, count)) {
            BioBuffer rejected{0, data, capacity};
            clearBuffer(rejected);
            free_nid_postfix(data);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        if (buffer.length != 0) std::memcpy(data, buffer.data, buffer.length);
        clearBuffer(buffer);
        free_nid_postfix(buffer.data);
        buffer.data = data;
        buffer.capacity = capacity;
    } else if (count != 0) {
        const auto start = reinterpret_cast<std::uintptr_t>(source);
        const auto begin = reinterpret_cast<std::uintptr_t>(buffer.data);
        if (start >= begin && start - begin <= buffer.capacity && count <= buffer.capacity - (start - begin))
            std::memmove(buffer.data + buffer.length, buffer.data + (start - begin), count);
        else if (!GuestCopyIn_nid_no_patch(buffer.data + buffer.length, source, count))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    buffer.length = length;
    return size;
}

int APS5_VABI putsMemory(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("BIO_puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeMemory(bio, text.data(), static_cast<int>(text.size()));
}

int APS5_VABI getsMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    if (!destination || size == std::numeric_limits<int>::min()) throw std::invalid_argument("BIO: invalid line buffer");
    bio->flags &= ~RetryFlags;
    constexpr char Terminator = '\0';
    if (size <= 1 || buffer.length == 0) {
        if (!GuestCopyOut_nid_no_patch(destination, &Terminator, 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 0;
    }
    const auto limit = std::min<std::size_t>(size - 1, buffer.length);
    std::vector<char> line(limit + 1);
    if (!GuestCopyIn_nid_no_patch(line.data(), buffer.data, limit)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const void* newline = std::memchr(line.data(), '\n', limit);
    const std::size_t count = newline ? static_cast<const char*>(newline) - line.data() + 1 : limit;
    line[count] = Terminator;
    if (!GuestCopyOut_nid_no_patch(destination, line.data(), count + 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    consumeMemory(bio, buffer, count);
    return static_cast<int>(count);
}

std::int64_t APS5_VABI controlMemory(Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) throw std::invalid_argument("BIO: null memory BIO");
    const Bio guestBio = ReadWritableBio(bio, __func__);
    switch (command) {
    case 1: {
        auto& buffer = memoryBuffer(bio);
        if (buffer.data) {
            if (guestBio.flags & ReadOnly) {
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
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &memoryBuffer(bio).data, sizeof(char*))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return static_cast<std::int64_t>(memoryBuffer(bio).length);
    case 8:
        return guestBio.shutdown;
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
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &guestBio.data, sizeof(guestBio.data))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 1;
    case 130:
        bio->number = static_cast<int>(argument);
        return 1;
    default:
        return 0;
    }
}

const BioMethod MemoryMethod{0x401, "memory buffer", writeMemory, readMemory, putsMemory, getsMemory, controlMemory, createMemory, destroyMemory, nullptr};
const BioMethod SslMethod{SslBioType, "ssl", writeSsl, readSsl, putsSsl, nullptr, controlSsl, createSsl, destroySsl, callbackControlSsl};

Bio* NewBio(const BioMethod* method, const char* caller) {
    if (!method) throw std::invalid_argument("BIO_new: null method");
    const BioMethod guestMethod = ReadBioMethod(method, caller);
    auto* bio = static_cast<Bio*>(calloc_nid_postfix(1, sizeof(Bio)));
    if (!bio) {
        queueError(BioLibrary, AllocationFailure, 0);
        return nullptr;
    }
    Bio initial{};
    initial.method = method;
    initial.shutdown = 1;
    initial.references = 1;
    if (!GuestCopyOut_nid_no_patch(bio, &initial, sizeof(initial))) {
        free_nid_postfix(bio);
        NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    }
    try {
        if (!guestMethod.create || guestMethod.create(bio)) return bio;
    } catch (...) {
        free_nid_postfix(bio);
        throw;
    }
    free_nid_postfix(bio);
    return nullptr;
}

int FreeBio(Bio* bio, const char* caller) {
    if (!bio) return 0;
    Bio guestBio = ReadWritableBio(bio, caller);
    if (guestBio.extraData) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    if (guestBio.references <= 1 && !guestBio.callback && guestBio.method)
        ReadBioMethod(guestBio.method, caller);
    if (DecrementReferences(bio) > 1) return 1;
    if (guestBio.callback) {
        const int result = static_cast<int>(guestBio.callback(bio, 1, nullptr, 0, 0, 1));
        if (result <= 0) return result;
        guestBio = ReadWritableBio(bio, caller);
        if (guestBio.extraData) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    }
    if (guestBio.method) {
        const BioMethod guestMethod = ReadBioMethod(guestBio.method, caller);
        if (guestMethod.destroy) guestMethod.destroy(bio);
    }
    free_nid_postfix(bio);
    return 1;
}

std::int64_t ControlBio(Bio* bio, int command, std::int64_t argument, void* pointer, const char* caller) {
    return DispatchControl(bio, command, argument, static_cast<const char*>(pointer),
        &BioMethod::control,
        [command, argument, pointer](auto fn, Bio* target) { return fn(target, command, argument, pointer); },
        caller);
}

int PutsBio(Bio* bio, const char* source, const char* caller) {
    if (!bio) return queueError(BioLibrary, UnsupportedMethod, -2);
    return DispatchOutput(bio, 4, source, 0, &BioMethod::puts,
        [source](auto fn, Bio* target) { return fn(target, source); },
        caller);
}

}  // namespace

extern "C" {

const LibreSsl::BioMethod* APS5_VABI BIO_s_mem_nid_postfix() {
    return &MemoryMethod;
}

const LibreSsl::BioMethod* APS5_VABI BIO_f_ssl_nid_postfix() {
    return &SslMethod;
}

LibreSsl::Bio* APS5_VABI BIO_new_nid_postfix(const LibreSsl::BioMethod* method) {
    return NewBio(method, __func__);
}

LibreSsl::Bio* APS5_VABI BIO_new_mem_buf_nid_postfix(const void* source, int size) {
    if (!source) {
        queueError(BioLibrary, NullParameter, 0);
        return nullptr;
    }
    auto length = static_cast<std::size_t>(size);
    if (size < 0) {
        std::string text;
        if (!CopyGuestStringPreservingErrno(static_cast<const char*>(source), text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        length = text.size();
    }
    auto* bio = NewBio(&MemoryMethod, __func__);
    if (!bio) return nullptr;
    auto& buffer = memoryBuffer(bio);
    buffer.data = const_cast<char*>(static_cast<const char*>(source));
    buffer.length = length;
    buffer.capacity = buffer.length;
    bio->flags |= ReadOnly;
    bio->number = 0;
    return bio;
}

int APS5_VABI BIO_free_nid_postfix(LibreSsl::Bio* bio) {
    return FreeBio(bio, __func__);
}

std::int64_t APS5_VABI BIO_ctrl_nid_postfix(LibreSsl::Bio* bio, int command, std::int64_t argument, void* pointer) {
    return ControlBio(bio, command, argument, pointer, __func__);
}

int LibreSslBioWrite_nid_no_patch(LibreSsl::Bio* bio, const void* source, int size) {
    if (!bio || size < 0) return 0;
    return DispatchOutput(bio, 3, static_cast<const char*>(source), size, &BioMethod::write,
        [source, size](auto fn, Bio* target) { return fn(target, static_cast<const char*>(source), size); },
        __func__);
}

int APS5_VABI BIO_puts_nid_postfix(LibreSsl::Bio* bio, const char* source) {
    return PutsBio(bio, source, __func__);
}

}
